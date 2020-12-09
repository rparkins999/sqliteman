/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2015 Richard P. Parkins, M. A., and released under the GPL.

It contains a parser for SQL schemas (not general SQL statements). Since the
schema is known to be valid, we do not detect all cases of bad syntax.

sqlite_master and describe table leave any quotes around types, but pragma table_info removes them.
*/

#include <QStringList>
#include <QMap>

#include "utils.h"
#include "sqlparser.h"

// state machine tokeniser
QList<Token> SqlParser::tokenise(QString input)
{
	static const QString hexDigit("0123456789ABCDEFabcdef");
	QList<Token> result = QList<Token>();
	while (!input.isEmpty())
	{
		Token t;
		t.type = tokenNone;
		int state = 0; // nothing
		while (1)
		{
			QChar c = input.isEmpty() ? 0 : input.at(0);
			switch (state)
			{
				case 0: // nothing
					if (c.isSpace())
					{
						input.remove(0, 1);
						continue;
					} // ignore it
					else if (c == '0')
					{
						state = 1; // had initial 0
					}
					else if (c.isDigit())
					{
						state = 3; // in number, no . or E yet
					}
					else if (c == '.')
					{
						state = 2; // had initial .
					}
					else if (c.toUpper() == 'X')
					{
						state = 8; // blob literal or identifier
					}
					else if (c.isLetter() || (c == '_'))
					{
						state = 10; // identifier
					}
					else if (c == '"')
					{
						t.name = QString(""); // empty, not null
						state = 11; // "quoted identifier"
						input.remove(0, 1);
						continue;
					}
					else if (c == '\'')
					{
						t.name = QString(""); // empty, not null
						state = 13; // 'string literal'
						input.remove(0, 1);
						continue;
					}
					else if (c == '|')
					{
						state = 15; // check for ||
					}
					else if (c == '<')
					{
						state = 16; // check for << <> <=
					}
					else if (c == '>')
					{
						state = 17; // check for >> >=
					}
					else if (c == '=')
					{
						state = 18; // check for ==
					}
					else if (c == '!')
					{
						state = 19; // !=
					}
					else if (c == '[')
					{
						t.name = QString(""); // empty, not null
						state = 20; // [quoted identifier]
						input.remove(0, 1);
						continue;
					}
					else if (c == '`')
					{
						t.name = QString(""); // empty, not null
						state = 21; // `quoted identifier`
						input.remove(0, 1);
						continue;
					}
					else if (c == 0) { break; }
					else
					{
						// single character token
						t.name.append(c);
						if (   (c == '*')
							|| (c == '/')
							|| (c == '%')
							|| (c == '+')
							|| (c == '-')
							|| (c == '&')
							|| (c == '~'))
						{ t.type = tokenOperator; }
						else { t.type = tokenSingle; }
						input.remove(0, 1);
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 1: // had initial 0
					if (c.isDigit())
					{
						state = 3; // in number, no . or E yet
					}
					else if (c.toUpper() == 'X')
					{
						state = 7; // in hex literal
					}
					else if (c == '.')
					{
						state = 4; // in number, had .
					}
					else if (c.toUpper() == 'E')
					{
						state = 5; // in number, just had E
					}
					else { // token is just 0
						t.type = tokenNumeric;
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 2: // had initial .
					if (c.isDigit())
					{
						state = 4; // in number, had .
					}
					else { // token is just .
						t.type = tokenSingle;
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 3: // in number, no . or E yet
					if (c == '.')
					{
						state = 4; // in number, had .
					}
					else if (c.toUpper() == 'E')
					{
						state = 5; // in number, just had E
					}
					else if (!c.isDigit())
					{
						t.type = tokenNumeric;
						break; // end of number
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 4: // in number, had .
					if (c.toUpper() == 'E')
					{
						state = 5; // in number, just had E
					}
					else if (!c.isDigit())
					{
						t.type = tokenNumeric;
						break; // end of number
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 5: // in number, just had E
					if (c.isDigit() || (c == '-') || (c == '+'))
					{
						state = 6; // in number after E
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					break; // actually invalid, but end number
				case 6: // in number after E
					if (c.isDigit())
					{
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					t.type = tokenNumeric;
					break; // end number
				case 7: // in hex literal
					if (hexDigit.contains(c))
					{
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					t.type = tokenNumeric;
					break; // end (hex) number
				case 8: // blob literal or identifier
					if (c == '\'')
					{
						state = 9; // blob literal
					}
					else if (c.isLetterOrNumber() || (c == '_') || (c == '$'))
					{
						state = 10; // identifier
					}
					else { // identifier just X
						t.type = tokenIdentifier;
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 9: // blob literal
					if (c == '\'')
					{
						t.name.append(c);
						input.remove(0, 1);
						t.type = tokenBlobLiteral;
						break; // end of blob literal
					}
					else if (hexDigit.contains(c))
					{
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					else { // invalid end of blob literal
						t.type = tokenBlobLiteral;
						break;
					}
				case 10: // identifier
					if (c.isLetterOrNumber() || (c == '_') || (c == '$'))
					{
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					else { // end of identifier
						if (operators.contains(t.name, Qt::CaseInsensitive))
						{ t.type = tokenOperator; }
						else if (posts.contains(t.name, Qt::CaseInsensitive))
						{ t.type = tokenPostfix; }
						else
						{ t.type = tokenIdentifier; }
						break;
					}
				case 11: // "quoted identifier"
					if (c == '"')
					{
						state = 12; // look for doubled "
					}
					else
					{
						t.name.append(c);
					}
					input.remove(0, 1);
					continue;
				case 12: // look for doubled "
					if (c == '"')
					{
						state = 11; // quoted identifier
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					else { // end of quoted identifier
						t.type = tokenQuotedIdentifier;
						break;
					}
				case 13: // 'string literal'
					if (c == '\'')
					{
						state = 14; // look for doubled '
					}
					else
					{
						t.name.append(c);
					}
					input.remove(0, 1);
					continue;
				case 14: // look for doubled '
					if (c == '\'')
					{
						state = 13; // string literal
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					else { // end of string literal
						t.type = tokenStringLiteral;
						break;
					}
				case 15: // check for ||
					if (c == '|')
					{
						t.name.append(c);
						input.remove(0, 1);
					}
					t.type = tokenOperator;
					break;
				case 16: // check for << <> <=
					if ((c == '<') || (c == '>') || (c == '='))
					{
						t.name.append(c);
						input.remove(0, 1);
					}
					t.type = tokenOperator;
					break;
				case 17: // check for >> >=
					if ((c == '>') || (c == '='))
					{
						t.name.append(c);
						input.remove(0, 1);
					}
					t.type = tokenOperator;
					break;
				case 18: // check for ==
					if (c == '=')
					{
						t.name.append(c);
						input.remove(0, 1);
					}
					t.type = tokenOperator;
					break;
				case 19: // !=
					// ! without = is invalid, but we treat it as a token
					t.name.append(c);
					input.remove(0, 1);
					t.type = tokenSingle;
					break;
				case 20: // [quoted identifier]
					if (c == ']')
					{
						t.type = tokenSquareIdentifier;
						input.remove(0, 1);
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 21: // `quoted identifier`
					if (c == '`')
					{
						state = 22; // look for doubled `
					}
					else
					{
						t.name.append(c);
					}
					input.remove(0, 1);
					continue;
				case 22:
					if (c == '`')
					{
						state = 21; // `quoted identifier`
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					else
					{
						t.type = tokenBackQuotedIdentifier;
						break;
					}
			}
			// if we didn't continue, we're at the end of the token
			break;
		}
		if (t.type != tokenNone) { result.append(t); }
	}
	return result;
}

void SqlParser::clearField(FieldInfo &f)
{
	f.name = QString();
	f.type = QString();
    f.referencedTable = QString();
    f.referencedKeys = QStringList(); 
	f.defaultValue = QString();
	f.defaultIsExpression = false;
	f.defaultisQuoted = false;
	f.isPartOfPrimaryKey = false;
	f.isWholePrimaryKey = false;
	f.isColumnPkDesc = false;
	f.isTablePkDesc = false;
	f.isAutoIncrement = false;
	f.isNotNull = false;
}

void SqlParser::addToPrimaryKey(FieldInfo &f)
{
	int PKCount = 1;
	f.isPartOfPrimaryKey = true;
	for (int i = 0; i < m_fields.count(); ++i)
	{
		if (m_fields[i].isPartOfPrimaryKey)
		{
			++PKCount;
		}
	}
	for (int i = 0; i < m_fields.count(); ++i)
	{
		if (m_fields[i].isPartOfPrimaryKey)
		{
			m_fields[i].isWholePrimaryKey = (PKCount == 1);
		}
	}
	f.isWholePrimaryKey = (PKCount == 1);
}

void SqlParser::addToPrimaryKey(QString s)
{
	int PKCount = 0;
	for (int i = 0; i < m_fields.count(); ++i)
	{
		if (s.compare(m_fields[i].name, Qt::CaseInsensitive) == 0)
		{
			m_fields[i].isPartOfPrimaryKey = true;
		}
		if (m_fields[i].isPartOfPrimaryKey)
		{
			++PKCount;
		}
	}
	for (int i = 0; i < m_fields.count(); ++i)
	{
		if (m_fields[i].isPartOfPrimaryKey)
		{
			m_fields[i].isWholePrimaryKey = (PKCount == 1);
		}
	}
}

void SqlParser::destroyExpression(Expression * e)
{
	if (e)
	{
		destroyExpression(e->left);
		destroyExpression(e->right);
		delete e;
	}
}

// removes some tokens from its argument
// we don't take account of precedence, since we won't try to evaluate it

Expression * SqlParser::parseExpression(QList<Token> & tokens, QStringList ends)
{
	Expression * expr = new Expression;
	expr->left = 0;
	expr->right = 0;
	int state = 0; // nothing
	while (!tokens.isEmpty())
	{
		QString s(tokens.at(0).name);
		switch (state)
		{
			case 0: // start of expression
				if (tokens.at(0).type == tokenOperator)
				{
					expr->type = exprOpX;
					expr->terminal = tokens.takeFirst();
					expr->right = parseExpression(tokens, ends);
					if (expr->right == 0)
					{
						destroyExpression(expr);
						return 0;
					}
					state = 2; // seen complete expression
					continue;
				}
				else if (tokens.at(0).type == tokenPostfix)
				{
					destroyExpression(expr);
					return 0;
				}
				else if (tokens.at(0).type == tokenSingle)
				{
					if (s.compare("(") == 0)
					{
						expr->type = exprExpr;
						++m_depth;
						tokens.removeFirst();
						expr->left = parseExpression(tokens, ends);
						if (expr->left == 0)
						{
							// empty expression
							destroyExpression(expr);
							return 0;
						}
						s = tokens.at(0).name;
						if (   (tokens.at(0).type == tokenSingle)
							&& (s.compare(")") == 0))
						{
							tokens.removeFirst();
							--m_depth;
							state = 2; // seen complete expression
							continue;
						}
						else
						{
							// ( not matched by )
							destroyExpression(expr);
							return 0;
						}
					}
					else if (   (s.compare(",") == 0)
							 || (s.compare(")") == 0))
					
					{
						// empty expression
						destroyExpression(expr);
						return 0;
					}
				}
				expr->type = exprToken;
				expr->terminal = tokens.takeFirst();
				state = 1; // seen token, check for function call
				continue;
			case 1: // seen token, check for function call
				if (   (tokens.at(0).type == tokenSingle)
					&& (s.compare("(") == 0))
				{
						Expression * e = new Expression;
						e->type = exprCall;
						++m_depth;
						e->left = expr;
						tokens.removeFirst();
						e->right = parseExpression(tokens, ends);
						if (e->right == 0)
						{
							destroyExpression(expr);
							return 0;
						}
						expr = e;
						s = tokens.at(0).name;
						if (   (tokens.at(0).type == tokenSingle)
							&& (s.compare(")") == 0))
						{
							tokens.removeFirst();
							--m_depth;
						}
						continue;
				}
				state = 2;
				// FALLTHRU
			case 2: // seen token or complete expression
				if (tokens.at(0).type == tokenOperator)
				{
					Expression * e = new Expression;
					e->type = exprXOpX;
					e->left = expr;
					e->terminal = tokens.takeFirst();
					e->right = parseExpression(tokens, ends);
					if (e->right == 0)
					{
						destroyExpression(expr);
						return 0;
					}
					expr = e;
					continue;
				}
				else if (tokens.at(0).type == tokenPostfix)
				{
					Expression * e = new Expression;
					e->type = exprXOp;
					e->left = expr;
					e->terminal = tokens.takeFirst();
					e->right = 0;
					expr = e;
					continue;
				}
				else if (tokens.at(0).type == tokenSingle)
				{
					if (s.compare(",") == 0)
					{
						if (m_depth == 0) { return expr;} // top level comma
						// treat embedded comma as operator
						Expression * e = new Expression;
						e->type = exprXOpX;
						e->left = expr;
						e->terminal = tokens.takeFirst();
						e->terminal.type = tokenOperator;
						e->right = parseExpression(tokens, ends);
						if (e->right == 0)
						{
							destroyExpression(e);
							return 0;
						}
						expr = e;
					}
					else if (s.compare(")") == 0)
					{
						return expr;
					}
				}
				else if (   (m_depth == 0)
					     && (tokens.at(0).type == tokenIdentifier)
						 && (ends.contains(s, Qt::CaseInsensitive)))
				{
					// expr terminated by reserved word
					return expr;
				}
				destroyExpression(expr);
				return 0; // expression followed by non-operator
		}
	}
	if (m_depth == 0)
	{
		return expr; // reached end
	}
	else
	{
		destroyExpression(expr);
		return 0;
	}
}

SqlParser::SqlParser(QString input)
{
	m_isValid = false;
	m_isUnique = false;
	m_hasRowid = true;
	m_whereClause = 0;
	operators << "AND" << "AS" << "BETWEEN" << "CASE" << "CAST" << "COLLATE"
			  << "DISTINCT" << "ELSE" << "ESCAPE" << "EXISTS" << "GLOB" << "IN"
			  << "IS" << "LIKE" << "MATCH" << "NOT" << "OR" << "REGEXP";
	posts << "ASC" << "DESC" << "END" << "ISNULL" << "NOTNULL";
	QList<Token> tokens = tokenise(input);
	m_depth = 0;
	int state = 0; // nothing
	FieldInfo f;
	clearField(f);
	while (!tokens.isEmpty())
	{
		QString s(tokens.at(0).name);
		switch (state)
		{
			case 0: // nothing
				if (s.compare("CREATE", Qt::CaseInsensitive) == 0)
				{
					state = 1; // seen CREATE
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 1: // seen CREATE
				// TEMP[ORARY] doesn't get copied to schema
				if (s.compare("TABLE", Qt::CaseInsensitive) == 0)
				{
					state = 2; // CREATE TABLE
					m_type = createTable;
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					m_isUnique = true;
					state = 82; // CREATE UNIQUE
				}
				else if (s.compare("INDEX", Qt::CaseInsensitive) == 0)
				{
					state = 83; // CREATE INDEX
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 2: // CREATE TABLE
				// IF NOT EXISTS doesn't get copied to schema
				// schema-name "." doesn't get copied to schema
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					m_tableName = s;
					state = 3; // had table name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 3:
				if (s.compare("(") == 0)
				{
					state = 4; // CREATE TABLE ( <columns...>
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 4: // look for a column definition
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					f.name = s;
					state = 6; // look for type name or constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 5: // look for column definition or table constraint
				if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 39; // look for table constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 41; // look for KEY
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 48; // look for columns and conflict clause
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 57; // look for bracketed expression
				}
				else if (s.compare("FOREIGN", Qt::CaseInsensitive) == 0)
				{
					state = 59; // look for KEY
				}
				else if (   (tokens.at(0).type == tokenIdentifier)
						 || (tokens.at(0).type == tokenQuotedIdentifier)
						 || (tokens.at(0).type == tokenSquareIdentifier)
						 || (tokens.at(0).type == tokenBackQuotedIdentifier)
						 || (tokens.at(0).type == tokenStringLiteral))
				{
					f.name = s;
					state = 6; // look for type name or constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 6: // look for type name or column constraint or , or )
				if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 5;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 79; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 8; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else if (   (tokens.at(0).type == tokenIdentifier)
						 || (tokens.at(0).type == tokenQuotedIdentifier)
						 || (tokens.at(0).type == tokenSquareIdentifier)
						 || (tokens.at(0).type == tokenBackQuotedIdentifier)
						 || (tokens.at(0).type == tokenStringLiteral))
				{
					f.type = s;
					state = 7; // look for constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 7: // look for column constraint or , or )
				if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 5; // look for column definition or table constraint
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 8; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 8: // look for column constraint name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 9; // look for constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 9: // look for constraint
				if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 10: // look for KEY
				if (s.compare("KEY", Qt::CaseInsensitive) == 0)
				{
					addToPrimaryKey(f);
					state = 11; // look for ASC or DESC or conflict clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 11: // look for ASC or DESC or conflict clause
				if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 12; // look for conflict clause
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					f.isColumnPkDesc = true;
					state = 12; // look for conflict clause
				}
				else if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 13; // look for CONFLICT
				}
				else if (s.compare("AUTOINCREMENT", Qt::CaseInsensitive) == 0)
				{
					//FIXME DESC or ASC is allowed after AUTOINCREMENT
					f.isAutoIncrement = true;
					state = 7; // look for column constraint or , or )
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 5;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 12: // look for conflict clause
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 13; // look for CONFLICT
				}
				else if (s.compare("AUTOINCREMENT", Qt::CaseInsensitive) == 0)
				{
					//FIXME DESC or ASC is allowed after AUTOINCREMENT
					f.isAutoIncrement = true;
					state = 7; // look for column constraint or , or )
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 5;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 13: // look for CONFLICT
				if (s.compare("CONFLICT", Qt::CaseInsensitive) == 0)
				{
					state = 14; // look for conflict action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 14: // look for conflict action
				if (s.compare("ROLLBACK", Qt::CaseInsensitive) == 0)
				{
					state = 15; // look for AUTOINCREMENT or next
				}
				else if (s.compare("ABORT", Qt::CaseInsensitive) == 0)
				{
					state = 15; // look for AUTOINCREMENT or next
				}
				else if (s.compare("FAIL", Qt::CaseInsensitive) == 0)
				{
					state = 15; // look for AUTOINCREMENT or next
				}
				else if (s.compare("IGNORE", Qt::CaseInsensitive) == 0)
				{
					state = 15; // look for AUTOINCREMENT or next
				}
				else if (s.compare("REPLACE", Qt::CaseInsensitive) == 0)
				{
					state = 15; // look for AUTOINCREMENT or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 15: // look for AUTOINCREMENT or next
				if (s.compare("AUTOINCREMENT", Qt::CaseInsensitive) == 0)
				{
					//FIXME DESC or ASC is allowed after AUTOINCREMENT
					f.isAutoIncrement = true;
					state = 7; // look for column constraint or , or )
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 5;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 16: // look for NULL
				if (s.compare("NULL", Qt::CaseInsensitive) == 0)
				{
					f.isNotNull = true;
					state = 15; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 17: // look for conflict clause or next
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 18; // look for CONFLICT
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 5;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 18: // look for CONFLICT
				if (s.compare("CONFLICT", Qt::CaseInsensitive) == 0)
				{
					state = 19; // look for conflict action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 19: // look for conflict action
				if (s.compare("ROLLBACK", Qt::CaseInsensitive) == 0)
				{
					state = 7; // look for constraint or , or )
				}
				else if (s.compare("ABORT", Qt::CaseInsensitive) == 0)
				{
					state = 7; // look for constraint or , or )
				}
				else if (s.compare("FAIL", Qt::CaseInsensitive) == 0)
				{
					state = 7; // look for constraint or , or )
				}
				else if (s.compare("IGNORE", Qt::CaseInsensitive) == 0)
				{
					state = 7; // look for constraint or , or )
				}
				else if (s.compare("REPLACE", Qt::CaseInsensitive) == 0)
				{
					state = 7; // look for constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 20: // look for bracketed expression
				if (s.compare("(") == 0)
				{
					state = 21; // look for end of expression
					++m_depth;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 21: // look for end of expression
				if (s.compare("(") == 0)
				{
					++m_depth;
				}
				else if (s.compare(")") == 0)
				{
					if (--m_depth == 0) { state = 7; }
				}
				tokens.removeFirst();
				continue;
			case 22: // look for default value
                if (tokens.at(0).type == tokenStringLiteral)
                {
					f.defaultisQuoted = true;
					f.defaultValue = s;
					state = 7; // look for column constraint or , or )
                }
				else if (s.compare("(") == 0)
				{
					f.defaultIsExpression = true;
					f.defaultValue = "(";
					state = 23; // scan for end of default value expression
					++m_depth;
				}
				else if (   (s.compare("+") == 0)
						 || (s.compare("-") == 0))
				{
					f.defaultValue = s;
					state = 24; // look for (signed) number
				}
				else
				{
					if (   (tokens.at(0).type == tokenQuotedIdentifier)
						|| (tokens.at(0).type == tokenSquareIdentifier)
						|| (tokens.at(0).type == tokenBackQuotedIdentifier))
					{
						f.defaultisQuoted = true;
					}
					f.defaultValue = s;
					state = 7; // look for column constraint or , or )
				}
				tokens.removeFirst();
				continue;
			case 23: // scan for end of default value expression
				if (s.compare("(") == 0)
				{
					f.defaultValue.append(s);
					++m_depth;
				}
				else if (s.compare(")") == 0)
				{
					f.defaultValue.append(s);
					if (--m_depth == 0) { state = 7; }
				}
				else
				{
					f.defaultValue.append(toString(tokens.at(0)));
				}
				tokens.removeFirst();
				continue;
			case 24: // look for (signed) number
				if (tokens.at(0).type == tokenNumeric)
				{
					f.defaultValue.append(s);
					state = 7; // look for column constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 25: // look for collation name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 7; // look for constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 26: // look for (foreign) table name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
                    f.referencedTable = s;
					state = 27; // look for clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 27: // look for column list or rest of foreign key clause
				if (s.compare("(") == 0)
				{
					state = 28; // look for column list
				}
				else if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 31; // look for DELETE or UPDATE
				}
				else if (s.compare("MATCH", Qt::CaseInsensitive) == 0)
				{
					state = 35; // look for SIMPLE or PARTIAL or FULL
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 36; // look for DEFERRABLE
				}
				else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 37; // look for INITIALLY or next
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 8; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 5;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 28: // look for column list
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
                    f.referencedKeys.append(s);
					state = 29; // look for next column name or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 29: // look for next column name or )
				if (s.compare(",") == 0)
				{
					state = 28; // look for column list
				}
				else if (s.compare(")") == 0)
				{
					state = 30; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 30: // look for rest of foreign key clause
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 31; // look for DELETE or UPDATE
				}
				else if (s.compare("MATCH", Qt::CaseInsensitive) == 0)
				{
					state = 35; // look for SIMPLE or PARTIAL or FULL
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 36; // look for DEFERRABLE
				}
				else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 37; // look for INITIALLY
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 8; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 5;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 31: // look for DELETE or UPDATE
				if (s.compare("DELETE", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for foreign key action
				}
				else if (s.compare("UPDATE", Qt::CaseInsensitive) == 0)
				{
					state = 32; // look for foreign key action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 32: // look for foreign key action
				if (s.compare("SET", Qt::CaseInsensitive) == 0)
				{
					state = 33; // look for NULL or DEFAULT
				}
				else if (s.compare("CASCADE", Qt::CaseInsensitive) == 0)
				{
					state = 30; // look for rest of foreign key clause
				}
				else if (s.compare("RESTRICT", Qt::CaseInsensitive) == 0)
				{
					state = 30; // look for rest of foreign key clause
				}
				else if (s.compare("NO", Qt::CaseInsensitive) == 0)
				{
					state = 34; // look for ACTION
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 33: // look for NULL or DEFAULT
				if (s.compare("NULL", Qt::CaseInsensitive) == 0)
				{
					state = 30; // look for rest of foreign key clause
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 30; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 34: // look for ACTION
				if (s.compare("ACTION", Qt::CaseInsensitive) == 0)
				{
					state = 30; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 35: // look for SIMPLE or PARTIAL or FULL
				if (s.compare("SIMPLE", Qt::CaseInsensitive) == 0)
				{
					state = 30; // look for rest of foreign key clause
				}
				else if (s.compare("PARTIAL", Qt::CaseInsensitive) == 0)
				{
					state = 30; // look for rest of foreign key clause
				}
				else if (s.compare("FULL", Qt::CaseInsensitive) == 0)
				{
					state = 30; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 36: // look for DEFERRABLE
				if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 37; // look for INITIALLY etc
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 37: // look for INITIALLY or next
				if (s.compare("INITIALLY", Qt::CaseInsensitive) == 0)
				{
					state = 38; // look for DEFERRED or IMMEDIATE
				}
				else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 8; // look for column constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 10; // look for KEY
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 16; // look for NULL
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 17; // look for conflict clause or next
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 20; // look for bracketed expression
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 22; // look for default value
				}
				else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 25; // look for collation name
				}
				else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 26; // look for (foreign) table name
				}
				else if (s.compare(",") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 5;
				}
				else if (s.compare(")") == 0)
				{
					m_fields.append(f);
					clearField(f);
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 38: // look for DEFERRED or IMMEDIATE
				if (s.compare("DEFERRED", Qt::CaseInsensitive) == 0)
				{
					state = 7; // look for column constraint or , or )
				}
				else if (s.compare("IMMEDIATE", Qt::CaseInsensitive) == 0)
				{
					state = 7; // look for column constraint or , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 39: // look for table constraint name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 40; // look for rest of table constraint
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 40: // look for rest of table constraint
				if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 41; // look for KEY
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 48; // look for columns and conflict clause
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 57; // look for bracketed expression
				}
				else if (s.compare("FOREIGN", Qt::CaseInsensitive) == 0)
				{
					state = 59; // look for KEY
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 41: // look for KEY
				if (s.compare("KEY", Qt::CaseInsensitive) == 0)
				{
					state = 42; // look for columns and conflict clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 42: // look for columns and conflict clause
				if (s.compare("(") == 0)
				{
					state = 43;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 43: // look for next column in list
				// sqlite doesn't currently support expressions here
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					addToPrimaryKey(s);
					state = 44; // look for COLLATE or ASC/DESC or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 44: // look for COLLATE or ASC/DESC or next
				if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 45; // look for collation name
				}
				else if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 47; // look for , or )
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					f.isTablePkDesc = true;
					state = 47; // look for , or )
				}
				else if (s.compare(",") == 0)
				{
					state = 43; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 54; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 45: // look for collation name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 46; // look for ASC/DESC or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 46: // look for ASC/DESC or next
				if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 47; // look for , or )
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					state = 47; // look for , or )
				}
				else if (s.compare(",") == 0)
				{
					state = 43; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 54; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 47: // look for , or )
				if (s.compare(",") == 0)
				{
					state = 43; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 54; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 48: // look for columns and conflict clause
				if (s.compare("(") == 0)
				{
					state = 49;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 49: // look for next column in list
				// sqlite doesn't currently support expressions here
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 50; // look for COLLATE or ASC/DESC or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 50: // look for COLLATE or ASC/DESC or next
				if (s.compare("COLLATE", Qt::CaseInsensitive) == 0)
				{
					state = 51; // look for collation name
				}
				else if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 53; // look for , or )
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					state = 53; // look for , or )
				}
				else if (s.compare(",") == 0)
				{
					state = 49; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 54; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 51: // look for collation name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 52; // look for ASC/DESC or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 52: // look for ASC/DESC or next
				if (s.compare("ASC", Qt::CaseInsensitive) == 0)
				{
					state = 53; // look for , or )
				}
				else if (s.compare("DESC", Qt::CaseInsensitive) == 0)
				{
					state = 53; // look for , or )
				}
				else if (s.compare(",") == 0)
				{
					state = 49; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 54; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 53: // look for , or )
				if (s.compare(",") == 0)
				{
					state = 49; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 54; // look for conflict clause or next
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 54: // look for conflict clause or next
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 55; // look for CONFLICT
				}
				else if (s.compare(",") == 0)
				{
					state = 78; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 79; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 55: // look for CONFLICT
				if (s.compare("CONFLICT", Qt::CaseInsensitive) == 0)
				{
					state = 56; // look for conflict action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 56: // look for conflict action
				if (s.compare("ROLLBACK", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for next table constraint or end
				}
				else if (s.compare("ABORT", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for next table constraint or end
				}
				else if (s.compare("FAIL", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for next table constraint or end
				}
				else if (s.compare("IGNORE", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for next table constraint or end
				}
				else if (s.compare("REPLACE", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for next table constraint or end
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 57: // look for bracketed expression
				if (s.compare("(") == 0)
				{
					state = 58; // look for end of expression
					++m_depth;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 58: // look for end of expression
				if (s.compare("(") == 0)
				{
					++m_depth;
				}
				else if (s.compare(")") == 0)
				{
					if (--m_depth == 0) { state = 77; }
				}
				tokens.removeFirst();
				continue;
			case 59: // look for KEY
				if (s.compare("KEY", Qt::CaseInsensitive) == 0)
				{
					state = 60; // look for column list
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 60: // look for column list
				if (s.compare("(") == 0)
				{
					state = 61; // look for next column in list
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 61: // look for next column in list
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 62; // look for , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 62: // look for , or )
				if (s.compare(",") == 0)
				{
					state = 61; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 63; // look for foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 63: // look for foreign key clause
				if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0)
				{
					state = 64; // look for table name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 64: // look for table name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 65; // look for column list or rest of clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 65: // look for column list or rest of clause
				if (s.compare("(") == 0)
				{
					state = 66; // look for next column in list
				}
				else if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 69; // look for DELETE or UPDATE
				}
				else if (s.compare("MATCH", Qt::CaseInsensitive) == 0)
				{
					state = 73; // look for SIMPLE or PARTIAL or FULL
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 74; // look for DEFERRABLE
				}
				else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 75; // look for INITIALLY
				}
				else if (s.compare(",") == 0)
				{
					state = 78; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 66: // look for next column in list
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					state = 67; // look for , or )
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 67:
				if (s.compare(",") == 0)
				{
					state = 66; // look for next column in list
				}
				else if (s.compare(")") == 0)
				{
					state = 68; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 68: // look for rest of foreign key clause
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 69; // look for DELETE or UPDATE
				}
				else if (s.compare("MATCH", Qt::CaseInsensitive) == 0)
				{
					state = 73; // look for SIMPLE or PARTIAL or FULL
				}
				else if (s.compare("NOT", Qt::CaseInsensitive) == 0)
				{
					state = 74; // look for DEFERRABLE
				}
				else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 75; // look for INITIALLY
				}
				else if (s.compare(",") == 0)
				{
					state = 78; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 81; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 69: // look for DELETE or UPDATE
				if (s.compare("DELETE", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for foreign key action
				}
				else if (s.compare("UPDATE", Qt::CaseInsensitive) == 0)
				{
					state = 70; // look for foreign key action
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 70: // look for foreign key action
				if (s.compare("SET", Qt::CaseInsensitive) == 0)
				{
					state = 71; // look for NULL or DEFAULT
				}
				else if (s.compare("CASCADE", Qt::CaseInsensitive) == 0)
				{
					state = 68; // look for rest of foreign key clause
				}
				else if (s.compare("RESTRICT", Qt::CaseInsensitive) == 0)
				{
					state = 68; // look for rest of foreign key clause
				}
				else if (s.compare("NO", Qt::CaseInsensitive) == 0)
				{
					state = 72; // look for ACTION
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 71: // look for NULL or DEFAULT
				if (s.compare("NULL", Qt::CaseInsensitive) == 0)
				{
					state = 68; // look for rest of foreign key clause
				}
				else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0)
				{
					state = 68; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 72: // look for ACTION
				if (s.compare("ACTION", Qt::CaseInsensitive) == 0)
				{
					state = 68; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 73: // look for SIMPLE or PARTIAL or FULL
				if (s.compare("SIMPLE", Qt::CaseInsensitive) == 0)
				{
					state = 68; // look for rest of foreign key clause
				}
				else if (s.compare("PARTIAL", Qt::CaseInsensitive) == 0)
				{
					state = 68; // look for rest of foreign key clause
				}
				else if (s.compare("FULL", Qt::CaseInsensitive) == 0)
				{
					state = 68; // look for rest of foreign key clause
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 74: // look for DEFERRABLE
				if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0)
				{
					state = 75; // look for INITIALLY etc
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 75: // look for INITIALLY or next
				if (s.compare("INITIALLY", Qt::CaseInsensitive) == 0)
				{
					state = 76; // look for DEFERRED or IMMEDIATE
				}
				else if (s.compare(",") == 0)
				{
					state = 78; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 79; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 76: // look for DEFERRED or IMMEDIATE
				if (s.compare("DEFERRED", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for next table constraint or end
				}
				else if (s.compare("IMMEDIATE", Qt::CaseInsensitive) == 0)
				{
					state = 77; // look for next table constraint or end
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 77: // look for next table constraint or end
				if (s.compare(",") == 0)
				{
					state = 78; // look for next table constraint
				}
				else if (s.compare(")") == 0)
				{
					state = 79; // check for WITHOUT ROWID or rubbish at end
					m_isValid = true;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 78: // look for next table constraint
				if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0)
				{
					state = 39; // look for table constraint name
				}
				else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0)
				{
					state = 41; // look for KEY
				}
				else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0)
				{
					state = 48; // look for columns and conflict clause
				}
				else if (s.compare("CHECK", Qt::CaseInsensitive) == 0)
				{
					state = 57; // look for bracketed expression
				}
				else if (s.compare("FOREIGN", Qt::CaseInsensitive) == 0)
				{
					state = 59; // look for KEY
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 79: // check for WITHOUT ROWID or rubbish at end
				m_isValid = false;
				if (s.compare("WITHOUT", Qt::CaseInsensitive) == 0)
				{
					state = 80; // seen WITHOUT
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 80: // seen WITHOUT
				if (s.compare("ROWID", Qt::CaseInsensitive) == 0)
				{
					state = 81; // seen ROWID
					m_isValid = true;
					m_hasRowid = false;
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 81: // seen ROWID, anything after it is an error
				m_isValid = false;
				break;
			case 82: // CREATE UNIQUE
				if (s.compare("INDEX", Qt::CaseInsensitive) == 0)
				{
					state = 83; // CREATE UNIQUE INDEX
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 83: // CREATE [UNIQUE] INDEX
				// IF NOT EXISTS doesn't get copied to schema
				// schema-name "." doesn't get copied to schema
				m_type = createIndex;
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					m_indexName = s;
					state = 84; // had index name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 84: // look for ON
				if (s.compare("ON", Qt::CaseInsensitive) == 0)
				{
					state = 85; // look for table name
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 85: // look for table name
				if (   (tokens.at(0).type == tokenIdentifier)
					|| (tokens.at(0).type == tokenQuotedIdentifier)
					|| (tokens.at(0).type == tokenSquareIdentifier)
					|| (tokens.at(0).type == tokenBackQuotedIdentifier)
					|| (tokens.at(0).type == tokenStringLiteral))
				{
					m_tableName = s;
					state = 86; // look for indexed column list
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 86: // look for indexed column list
				if (s.compare("(") == 0)
				{
					state = 87; // look for indexed column
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 87: // look for indexed column
			{
				Expression * expr = parseExpression(tokens, QStringList());
				if (   (expr == 0)
					|| (tokens.at(0).type != tokenSingle))
				{ break ; } // not a valid create statement
				m_columns.append(expr);
				s = tokens.at(0).name;
				if (s.compare(")") == 0)
				{
					m_isValid = true;
					state = 88; // look for end or WHERE clause
				}
				else if (s.compare(",") == 0) {;} // look for another column
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			}
			case 88: // look for end or WHERE clause
				if (s.compare("WHERE", Qt::CaseInsensitive) == 0)
				{
					state = 89; // look for expression
				}
				else { break; } // not a valid create statement
				tokens.removeFirst();
				continue;
			case 89: // look for expression
				m_isValid = false;
				m_whereClause = parseExpression(tokens, QStringList());
				if (m_whereClause != 0)
				{
					m_isValid = true;
					state = 90; // look for rubbish after end
				}
				else { break; } // not a valid create statement
				if (!tokens.isEmpty())
				{
					tokens.removeFirst();
				}
				continue;
			case 90: // rubbish after end
				m_isValid = false;
				break;
		}
		break;
	}
}

SqlParser::~SqlParser()
{
	while (!m_columns.isEmpty())
	{
		destroyExpression(m_columns.takeFirst());
	}
	destroyExpression(m_whereClause);
}


QString SqlParser::defaultToken(FieldInfo &f)
{
	QString result(f.defaultValue);
	if (result.isEmpty()) { return QString(); }
	else if (f.defaultisQuoted)
	{
		return Utils::q(result, "'");
	}
	else {return result; }
}

QString SqlParser::toString(Token t)
{
	QString s(" ");
	switch (t.type)
	{
		case tokenQuotedIdentifier:
			return s + Utils::q(t.name);
		case tokenSquareIdentifier:
			return s + "[" + t.name + "]";
		case tokenBackQuotedIdentifier:
			return s + Utils::q(t.name, "`");
		case tokenStringLiteral:
			return s + Utils::q(t.name, "'");
		case tokenBlobLiteral:
		case tokenIdentifier:
		case tokenNumeric:
		case tokenOperator:
		case tokenPostfix:
		case tokenSingle:
			return s + t.name;
		default:
			return QString();
	}
}

QString SqlParser::toString(Expression * expr)
{
	if (expr == 0) { return "<null expression>"; }
	QString s(" ");
	switch (expr->type)
	{
		case exprToken: // a single operand
			return toString(expr->terminal);
		case exprXOpX: // expression binary-operator expression
			s += toString(expr->left);
			/*FALLTHRU*/
		case exprOpX: // unary-prefix-operator expression
			s += toString(expr->terminal);
			return s + toString(expr->right);
		case exprXOp: // expression unary-postfix-operator
			s += toString(expr->left);
			return s +toString( expr->terminal);
		case exprCall: // fname (expression)
			s += toString(expr->left) + "(";
			return s + toString(expr->right) + " )";
		case exprExpr: // ( expression )
			return s + "(" + toString(expr->left) + " )";
	}
	return QString();
}

QString SqlParser::toString()
{
	QString s("CREATE ");
	switch (m_type)
	{
		case createTable:
		{
			QStringList pk;
			s += m_tableName;
			s += " (";
			bool first = true;
			foreach (FieldInfo f, m_fields)
			{
				if (first)
				{
					first = false;
					s += Utils::q(f.name);
				}
				else
				{
					s += ", " + Utils::q(f.name);
				}
				if (!f.type.isEmpty())
				{
					s += " " + Utils::q(f.type);
				}
				if (!f.defaultValue.isEmpty())
				{
					if (f.defaultisQuoted)
					{
						s += " DEFAULT " + Utils::q(f.defaultValue, "'");
					}
					else
					{
						s += " DEFAULT " + f.defaultValue;
					}
				}
				if (f.isNotNull) { s += " NOT NULL"; }
				if (f.isAutoIncrement) { s += " PRIMARY KEY AUTOINCREMENT"; }
				else if (f.isPartOfPrimaryKey)
				{
					pk.append(Utils::q(f.name));
				}
			}
			if (!pk.isEmpty()) { s += pk.join(", "); }
			if (m_hasRowid) { s += ") ;"; }
			else { s += ") WITHOUT ROWID ;"; }
		}
		break;
		case createIndex:
		{
			if (m_isUnique) { s += "UNIQUE "; }
			s += "INDEX " + Utils::q(m_indexName);
			s += " ON " + Utils::q(m_tableName) + " (\n";
			bool first = true;
			foreach (Expression * expr, m_columns)
			{
				if (first) { first = false; }
				else { s += ",\n"; }
				s += toString(expr);
			}
			s += ")";
			if (m_whereClause)
			{
				s += "\nWHERE " + toString(m_whereClause);
			}
			s += " ;";
		}
		break;
	}
	return s;
}

bool SqlParser::replaceToken(QMap<QString,QString> map, Expression * expr)
{
	QString s(expr->terminal.name);
	if (map.contains(s))
	{
		QString t(map.value(s));
		if (t.isNull()) { return false; } // column removed
		expr->terminal.name = t;
		expr->terminal.type = tokenQuotedIdentifier;
	}
	return true;
}

/* Note the rules here for which tokens are considered to be column names are
 * not as described in www.sqlite.org/lang_keywords.html: they are
 * determined by experiment.
 * token type              contexts recognised as a column name
 *             indexed-column  indexed-column  ordering-term  WHERE-expression
 *              at top level   in expression
 * number           no              no              (1)             no
 * 'number'         yes             no              (2)             no
 * "number"         yes             yes             yes             yes
 * `number`         yes             yes             yes             yes
 * [number]         yes             yes             yes             yes
 * (number)         no                              (1)             no
 * ('number')       yes                             (2)             no
 * ("number")       yes                             yes             yes
 * (`number`)       yes                             yes             yes
 * ([number])       yes                             yes             yes
 * name (3)         yes             yes             yes             yes
 * 'name' (3)       yes             no              no              no
 * "name" (3)       yes             yes             yes             yes
 * `name` (3)       yes             yes             yes             yes
 * (name) (3)       yes                             yes             yes
 * ('name') (3)     yes                             no              no
 * ("name") (3)     yes                             yes             yes
 * (`name`) (3)     yes                             yes             yes
 * ([name]) (3)     yes                             yes             yes
 * name (4)         (5)             (5)             (5)             no
 * 'name' (4)       yes             no              no              no
 * "name" (4)       yes             yes             yes             yes
 * `name` (4)       yes             yes             yes             yes
 * [name] (4)       yes             yes             yes             yes
 * (name) (4)       (5)
 * ('name') (4)     yes                             no              no
 * ("name") (4)     yes                             yes             yes
 * (`name`) (4)     yes                             yes             yes
 * ([name]) (4)     yes                             yes             yes
 * null (6)         no             no               no              no
 * 'null' (6)       yes            no               no              no
 * "null" (6)       yes            yes              yes             yes
 * `null` (6)       yes            yes              yes             yes
 * [null] (6)       yes            yes              yes             yes
 * (null) (6)       no                              no              no
 * ('null') (6)     yes                             no              no
 * ("null") (6)     yes                             yes             yes
 * (`null`) (6)     yes                             yes             yes
 * ([null]) (6)     yes                             yes             yes
 *
 * (1) bare number as order by expression is treated as a column number
 * (2) only if the indexed-column is the same 'number' as well
 * (3) not reserved word, or reserved word allowed as unquoted column name
 * (4) reserved word not allowed as unquoted column name
 * (5) create index fails, so this case can't occur in the schema
 * (6) the exact identifier null
 */
bool SqlParser::replace(QMap<QString,QString> map,
						Expression * expr, bool inside)
{
	if (expr)
	{
		switch (expr->type)
		{
			case exprToken: // a single operand
				switch (expr->terminal.type)
				{
					case tokenIdentifier:
						if (expr->terminal.name.compare(
							"NULL", Qt::CaseInsensitive) == 0)
							{ return true; }
						else {return replaceToken(map, expr); }
					case tokenStringLiteral:
						if (inside) { return true; }
						else { return replaceToken(map, expr); }
					case tokenQuotedIdentifier:
					case tokenSquareIdentifier:
					case tokenBackQuotedIdentifier:
					case tokenSingle:
						return replaceToken(map, expr);
					case tokenNumeric:
					case tokenBlobLiteral:
					case tokenOperator:
					case tokenPostfix:
						return true;
				}
			case exprXOpX: // expression binary-operator expression
				return    replace(map, expr->left, true)
					   && replace(map, expr->right, true);
			case exprOpX: // unary-prefix-operator expression
				return replace(map, expr->right, true);
			case exprXOp: // expression unary-postfix-operator
				return replace(map, expr->left, true);
			case exprCall: // fname (expression)
				return replace(map, expr->right, true);
			case exprExpr: // ( expression )
				return replace(map, expr->left, inside);
		}
	}
	return true;
}

bool SqlParser::replace(QMap<QString,QString> map, QString newTableName)
{
	m_tableName = newTableName;
	bool result = true;
	foreach (Expression * expr, m_columns)
	{
		result &= replace(map, expr, false);
	}
	return result & replace(map, m_whereClause, true);
}
