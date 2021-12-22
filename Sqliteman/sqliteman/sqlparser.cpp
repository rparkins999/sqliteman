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

#include "pd.h" // debugging
#include "sqlparser.h"
#include "utils.h"

namespace { // local to this file (and therefore static)
    // identifiers which are postfix operators
    QStringList posts({"ASC", "DESC", "END", "ISNULL", "NOTNULL" });
    // identifiers which are prefix operators
    QStringList prefixes({"CASE", "CAST", "DISTINCT", "EXISTS" });
    // identifiers which are binary operators
    QStringList operators({"AND", "AS", "BETWEEN", "COLLATE",
                           "ELSE", "ESCAPE", "GLOB", "IN", "IS",
                           "LIKE", "MATCH", "OR", "REGEXP", "THEN", "WHEN"});
    // identifiers which can combine after NOT
    QStringList afterNOTs({"BETWEEN", "GLOB", "IN", "LIKE",
                           "MATCH", "REGEXP"});
    // used to save creating one whenever needed
    Expression nullExpression = {exprNull, NULL, {"", tokenNone}, NULL};
    // used to save creating one whenever needed
    Token nullToken = {"", tokenNone};
    // Last token used as argument for tosString(),
    // used to insert spaces when needed to separate tokens.
    Token m_lastToString;
}

SqlParser::SqlParser() {
    m_fields = QList<FieldInfo>();
    m_columns = QList<Expression *>();
    m_whereClause = NULL;
    m_tokens = QList<Token>();
    m_lastToString = nullToken;
}

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
					if (c.isSpace()) {
						input.remove(0, 1);
						continue; // ignore it
					} else if (c == '0') {
                        t.type = tokenNumeric;
						state = 1; // had initial 0
					} else if (c.isDigit()) {
                        t.type = tokenNumeric;
						state = 3; // in number, no . or E yet
					} else if (c == '.') {
                        t.type = tokenNumeric;
						state = 2; // had initial .
					} else if (c.toUpper() == 'X') {
						state = 8; // blob literal or identifier
					} else if (c.isLetter() || (c == '_')) {
                        t.type = tokenIdentifier;
						state = 10; // identifier
					} else if (c == '"') {
						t.name = QString(""); // empty, not null
                        t.type = tokenQuotedIdentifier;
						state = 11; // "quoted identifier"
						input.remove(0, 1);
						continue;
					} else if (c == '\'') {
						t.name = QString(""); // empty, not null
                        t.type = tokenStringLiteral;
						state = 13; // 'string literal'
						input.remove(0, 1);
						continue;
					} else if (c == '|') {
						state = 15; // check for ||
					} else if (c == '<') {
						state = 16; // check for << <> <=
					} else if (c == '>') {
						state = 17; // check for >> >=
					} else if (c == '=') {
						state = 18; // check for ==
					} else if (c == '!') {
						state = 19; // check for !=
					} else if (c == '[') {
						t.name = QString(""); // empty, not null
                        t.type = tokenSquareIdentifier;
						state = 20; // [quoted identifier]
						input.remove(0, 1);
						continue;
					} else if (c == '`') {
						t.name = QString(""); // empty, not null
                        t.type = tokenBackQuotedIdentifier;
						state = 21; // `quoted identifier`
						input.remove(0, 1);
						continue;
					} else if (c == 0) {
                        break;
					} else {
						// single character token
                        if (c == '/') {
                            input.remove(0, 1);
                            if (!input.isEmpty()) {
                                if (input.at(0) == '*') {
                                    // /* comment, skip until */
                                    bool star = false;
                                    while (!input.isEmpty())
                                    {
                                        c = input.at(0);
                                        input.remove(0, 1);
                                        if (star) {
                                            if (c == '/') { break ; }
                                            star = false;
                                        } else {
                                        }
                                        star = (c == '*');
                                    }
                                    continue;
                                }
                            }
                            t.type = tokenOperator;
                            t.name = "/";
                            break;
                        } else if (   (c == '*')
                                   || (c == '%')
                                   || (c == '&'))
						{ t.type = tokenOperator; }
						else if (c == '~')
						{ t.type = tokenPrefix; }
						else if (c == '(')
						{ t.type = tokenOpen; }
                        else if (c == ',')
						{ t.type = tokenComma; }
                        else if (c == ')')
						{ t.type = tokenClose; }
						else if (c == '+')
						{ t.type = tokenPlusMinus; }
						else if (c == '-') {
                            input.remove(0, 1);
                            if (!input.isEmpty()) {
                                if (input.at(0) == '-') {
                                    // -- comment, skip until end of line
                                    while (!input.isEmpty())
                                    {
                                        c = input.at(0);
                                        input.remove(0, 1);
                                        if (c == '\n') { break ; }
                                    }
                                    continue;
                                }
                            }
                            t.type = tokenPlusMinus;
                            t.name = "-";
                            break;
                        }
						else { t.type = tokenInvalid; }
						t.name.append(c);
						input.remove(0, 1);
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 1: // had initial 0
					if (c.isDigit()) {
						state = 3; // in number, no . or E yet
					} else if (c.toUpper() == 'X') {
						state = 7; // in hex literal
					} else if (c == '.') {
						state = 4; // in number, had .
					} else if (c.toUpper() == 'E') {
						state = 5; // in number, just had E
					} else { // token is just 0
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 2: // had initial .
					if (c.isDigit()) {
						state = 4; // in number, had .
					} else { // token is just .
						t.type = tokenInvalid;
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 3: // in number, no . or E yet
					if (c == '.') {
						state = 4; // in number, had .
					} else if (c.toUpper() == 'E') {
						state = 5; // in number, just had E
					} else if (!c.isDigit()) {
						break; // end of number
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 4: // in number, had .
					if (c.toUpper() == 'E') {
						state = 5; // in number, just had E
                    } else if (c == '.' ) {
						t.type = tokenInvalid;
					} else if (!c.isDigit()) {
                        break; // end of number
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 5: // in number, just had E
					if (c.isDigit() || (c == '-') || (c == '+')) {
						state = 6; // in number after E
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					t.type = tokenInvalid;
					break; // invalid, end number
				case 6: // in number after E
					if (c.isDigit()) {
						t.name.append(c);
						input.remove(0, 1);
						continue;
					} else if ((c == '.') || (c.toLower() == 'e')) {
                        t.type = tokenInvalid;
						t.name.append(c);
						input.remove(0, 1);
						continue;
                    }
					break; // end number
				case 7: // in hex literal
					if (hexDigit.contains(c)) {
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					break; // end (hex) number
				case 8: // blob literal or identifier
					if (c == '\'') {
						state = 9; // blob literal
						t.type = tokenBlobLiteral;
					}
					else if (c.isLetterOrNumber() || (c == '_') || (c == '$'))
					{
						t.type = tokenIdentifier;
						state = 10; // identifier
					} else { // identifier just X
						t.type = tokenIdentifier;
						break;
					}
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 9: // blob literal
					if (c == '\'') {
						t.name.append(c);
						input.remove(0, 1);
						break; // end of blob literal
					} else if (!hexDigit.contains(c)) {
                        // invalid character in blob literal
						t.type = tokenInvalid;
                        if (c == 0) { break; } // unterminated
					}
                    t.name.append(c);
                    input.remove(0, 1);
                    continue;
				case 10: // identifier
					if (c.isLetterOrNumber() || (c == '_') || (c == '$')) {
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					// end of identifier
                    if (operators.contains(t.name, Qt::CaseInsensitive))
                    { t.type = tokenOperatorA; }
                    else if (posts.contains(t.name, Qt::CaseInsensitive))
                    { t.type = tokenPostfixA; }
                    else if (prefixes.contains(t.name, Qt::CaseInsensitive))
                    { t.type = tokenPrefixA; }
                    else if (t.name.compare("NOT", Qt::CaseInsensitive) == 0)
                    { t.type = tokenNOT; }
                    break;
                case 11: // "quoted identifier"
					if (c == '"') {
						state = 12; // look for doubled "
                    } else if (c == 0) {
						t.type = tokenInvalid;
                        break; // unterminated "quoted identifier"
					} else { t.name.append(c); }
					input.remove(0, 1);
					continue;
				case 12: // look for doubled "
					if (c == '"') {
						state = 11; // still in "quoted identifier"
						t.name.append(c);
						input.remove(0, 1);
						continue;
                    }
					break; // end of quoted identifier
				case 13: // 'string literal'
					if (c == '\'')
					{
						state = 14; // look for doubled '
					} else if (c == 0) {
						t.type = tokenInvalid;
                        break; // unterminated 'string literal'
                    } else {
						t.name.append(c);
					}
					input.remove(0, 1);
					continue;
				case 14: // look for doubled '
					if (c == '\'') {
						state = 13; // string literal
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
					break; // end of string literal
				case 15: // check for ||
					if (c == '|') {
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
				case 19: // had !, check for !=
					if (c == '=') { t.type = tokenOperator; }
					else { t.type = tokenInvalid; }
                    t.name.append(c);
                    input.remove(0, 1);
					break;
				case 20: // [quoted identifier]
					if (c == ']') {
						input.remove(0, 1);
						break;
					} else if (c == 0) {
                        t.type = tokenInvalid;
                        break; // unterminated [quoted identifier]
                    }
					t.name.append(c);
					input.remove(0, 1);
					continue;
				case 21: // `quoted identifier`
					if (c == '`') {
						state = 22; // look for doubled `
					} else if (c == 0) {
                        t.type = tokenInvalid;
                        break; // unterminated `quoted identifier`
                    } else { t.name.append(c); }
					input.remove(0, 1);
					continue;
				case 22:
					if (c == '`') {
						state = 21; // `quoted identifier`
						t.name.append(c);
						input.remove(0, 1);
						continue;
					}
                    break;
			}
			// if we didn't continue, we're at the end of the token
			break;
		}
		if (t.type != tokenNone) {
            result.append(t);
            state = 0;
        }
	}
	return result;
}

// Stringify token, inserting space if necessary to separate from previous one
QString SqlParser::tos(const Token t)
{
    QString result;
	switch (t.type)
	{
		case tokenQuotedIdentifier:
            if (m_lastToString.type == tokenQuotedIdentifier)
            { result = " " + Utils::q(t.name); }
            else { result = Utils::q(t.name); }
            break;
		case tokenSquareIdentifier:
			result = "[" + t.name + "]"; break;
		case tokenBackQuotedIdentifier:
            if (m_lastToString.type == tokenBackQuotedIdentifier)
            { result = " " + Utils::q(t.name, "`"); }
            else { result = Utils::q(t.name, "`"); }
            break;
		case tokenStringLiteral:
            if (   (m_lastToString.type == tokenStringLiteral)
                || (m_lastToString.type == tokenBlobLiteral))
            { result = " " + Utils::q(t.name, "'"); }
            else { result = Utils::q(t.name, "'"); }
            break;
		case tokenBlobLiteral:
            if (   (m_lastToString.type == tokenStringLiteral)
                || (m_lastToString.type == tokenBlobLiteral))
            { result = " " + t.name; }
            else { result = t.name; }
            break;
		case tokenIdentifier:
		case tokenNumeric:
		case tokenOperatorA:
		case tokenPostfixA:
		case tokenPrefixA:
		case tokenNOT:
            if (   (m_lastToString.type == tokenIdentifier)
                || (m_lastToString.type == tokenNumeric)
                || (m_lastToString.type == tokenOperatorA)
                || (m_lastToString.type == tokenPostfixA)
                || (m_lastToString.type == tokenPrefixA)
                || (m_lastToString.type == tokenNOT))
            { result = " " + t.name; }
            else { result = t.name; }
            break;
		case tokenOpen:
		case tokenComma:
		case tokenClose:
            result = t.name; break;
		case tokenOperator:
		case tokenPrefix:
		case tokenPlusMinus:
            if (   (m_lastToString.type == tokenOperator)
                || (m_lastToString.type == tokenPrefix)
                || (m_lastToString.type == tokenPlusMinus))
            { result = " " + t.name; }
            else { result = t.name; }
            break;
		default:
			result = "";
	}
	m_lastToString = t;
    return result;
}

void SqlParser::destroyExpression(Expression * e)
{
	if (e == NULL) { return; }
    switch (e->type) {
        case exprNull: // empty argument list
            return; // static, don't try to free it
		case exprToken: // a single operand
			break;
		case exprOpX: // unary-prefix-operator expression
            destroyExpression(e->right); break;
		case exprXOpX: // expression binary-operator expression
            destroyExpression(e->left); destroyExpression(e->right); break;
		case exprXOp: // expression unary-postfix-operator
		case exprExpr: // ( expression )
			destroyExpression(e->left); break;
		case exprCall: // fname (expression)
            destroyExpression(e->right); break;
	}
    delete e;
}

// Parse expression from m_tokens with initial stats "state"
// and expecting something from "ends" after the end of it.
// Removes what it succesfully parses from m_tokens.
// Returns NULL if the expression is invalid
// or an Expression which is the root of the parse tree.
// For the time being we don't handle some cases which are invalid
// in a DEFAULT expression.
// We don't worry about precedence because we aren't evaluating it
Expression * SqlParser::internalParser(
    int state, int level, QStringList ends)
{
    // level 0 = top level
    // level 1 = in argument list of function call
    // level 2 = inside () nested expression
    // level 3 = expecting specific terminator
    // levels 1 and 2 can be any recursion depth
    Expression * expr;
    Token t;
    while (true) { // loop until we find an expression terminator
        switch (state) {
            case 0: // at start of expression, expecting an operand
                    // returns entire expression at this recursion depth
                if (m_tokens.isEmpty()) { // empty expression
                    if (level == 0) { // OK at top level
                        return &nullExpression;
                    } else { return NULL; } // unmatched '('
                } else if (   (level == 0)
                           && (m_tokens.at(0).type == tokenIdentifier)
                           && ends.contains(
                                m_tokens.at(0).name, Qt::CaseInsensitive))
                { // empty expression is valid at level 0
                    return &nullExpression;
                } else {
                    t = m_tokens.takeFirst(); // m_tokens isn't empty now
                }
                if (t.type == tokenWhitespace) {
                    continue;
                } else if (t.type == tokenClose) {
                    if (level == 1) { // empty arglist
                        return &nullExpression;
                    } else { return NULL; } // "()" expression or unmatched ')'
                } else if (level == 1) { // in function call
                    if (   (t.type == tokenIdentifier)
                        && (t.name.compare(
                            "DISTINCT", Qt::CaseInsensitive) == 0))
                    {
                        // DISTINCT is a keyword here
                        // Treat it as a prefix operator
                        // but don't allow DISTINCT DISTINCT
                        Expression * expr = new Expression;
                        expr->type = exprOpX;
                        expr->terminal.type = tokenPrefixA;
                        expr->terminal.name = t.name;
                        expr->right = internalParser(0, 1, QStringList());
                        if (   (expr->right == NULL)
                            || (   (expr->right->type == exprOpX)
                                && (expr->right->terminal.type
                                        == tokenPrefixA)
                                && (expr->right->terminal.name.compare(
                                    "DISTINCT", Qt::CaseInsensitive) == 0)))
                        {
                            destroyExpression(expr);
                            return NULL;
                        } else { return expr; }
                    } else if (   (t.type == tokenOperator)
                               && (t.name == "*")
                               && (m_tokens.size() > 0)
                               && (m_tokens.at(0).type == tokenClose))
                    { // (*) is a valid function argument list
                        m_tokens.removeFirst();
                        Expression * expr = new Expression;
                        expr->type = exprToken;
                        expr->terminal.type = tokenIdentifier;
                        expr->terminal.name = t.name;
                        return expr;
                    }
                } else if (level == 2) { // in bracketed expression
                    if (t.name.compare("SELECT", Qt::CaseInsensitive) == 0){
                        // For the time being we don't handle a subquery
                        // because it is not allowed in a DEFAULT expression
                        // even if it returns a constant result.
                        return NULL;
                    }
                }
                state = 2;
                continue;
            case 1: // recursive call after prefix or binary operator or comma
                    // expecting an operand
                    // returns entire expression at this recursion depth
                if (m_tokens.isEmpty()) { // premature end of expression
                    return NULL;
                } else if (   (m_tokens.at(0).type == tokenIdentifier)
                           && ends.contains(
                               m_tokens.at(0).name, Qt::CaseInsensitive))
                {
                    if ((level == 0) || (level == 3)) {
                        return NULL; // premature end of expression
                    }
                } else {
                    t = m_tokens.takeFirst(); // m_tokens isn't empty now
                }
                if (t.type == tokenWhitespace) {
                    continue;
                } else if (t.type == tokenClose) {
                    return NULL; // premature end of expression
                }
                /*FALLTHRU*/ // no need to set state, it isn't read
            case 2: // expecting an operand, t has a token
                    // returns entire expression at this recursion depth
                switch (t.type) {
                    case tokenIdentifier:
                        if (   (m_tokens.size() > 0)
                            && (m_tokens.at(0).type == tokenOpen))
                        { // We assume here that a user defined function name
                          // must be a valid unquoted identifier, although the
                          // sqlite documentation doesn't explicitly say this.
                            m_tokens.removeFirst();
                            expr = new Expression();
                            expr->type = exprCall; // function call
                            expr->terminal = t; // function name
                            expr->right = internalParser(0, 1, QStringList());
                            if (expr->right == NULL) { // invalid expression
                                destroyExpression(expr);
                                return NULL;
                            } else { // recursive call found (arglist)
                                break; // got operand, fall into state 3
                            }
                        }
                        /*FALLTHRU*/
                    case tokenQuotedIdentifier:
                    case tokenSquareIdentifier:
                    case tokenBackQuotedIdentifier:
                    case tokenStringLiteral:
                    case tokenBlobLiteral:
                    case tokenNumeric:
                        expr = new Expression();
                        expr->type = exprToken;
                        expr->terminal = t;
                        break; // got operand, fall into state 3
                    case tokenPrefix:
                    case tokenPrefixA:
                    case tokenPlusMinus: // prefixes
                        if (t.name.compare(
                            "CASE", Qt::CaseInsensitive) == 0)
                        {
                            QString sp;
                            if (m_tokens.isEmpty()) { return NULL; }
                            else if (m_tokens.at(0).type == tokenWhitespace) {
                                sp = m_tokens.takeFirst.name;
                                if (m_tokens.isEmpty()) { return NULL; }
                            } else if (m_tokens.at(0).name.compare(
                                     "WHEN", Qt::CaseInsensitive) == 0)
                            {
                                m_tokens.removeFirst();
                                t.name = "CASE WHEN"; // treat as single prefix
                            }
                            // fall though into normal prefix handling
                        } else if (t.name.compare(
                                   "CAST", Qt::CaseInsensitive) == 0)
                        {
                            if (   m_tokens.isEmpty()
                                || (m_tokens.at(0).type != tokenOpen))
                            {
                                return NULL; // CAST not followed by '('
                            } // otherise fall into standard prefix code
                            expr = new Expression(); // CAST expression
                            expr->type = exprCall; // looks like a call
                            expr->terminal = t;
                            m_tokens.removeFirst(); // absorb '('
                            expr->right = internalParser(4, level, ends);
                            state = 3; // got operand, look for operator or end
                            continue;
                        }
                        // recurse to check for multiple prefixes
                        expr = new Expression();
                        expr->type = exprOpX;
                        expr->terminal = t;
                        expr->right = internalParser(1, level, ends);
                        if (expr->right == NULL) { // invalid expression
                            destroyExpression(expr);
                            return NULL;
                        } else { // recursive call found rest of expression
                            return expr; // done at this recursion depth
                        }
                    case tokenOpen: // operand is (expression)
                        expr = new Expression();
                        expr->type = exprExpr;
                        expr->left = internalParser(0, 2, QStringList());
                        if (expr->left == NULL) { // invalid expression
                            destroyExpression(expr);
                            return 0;
                        } else { // recursive call found (expression)
                            state = 3; // got operand, look for operator or end
                            continue;
                        }
                    case tokenNOT: // NOT and NOT EXISTS are prefixes
                        expr = new Expression();
                        expr->type = exprOpX;
                        expr->terminal.type = tokenPrefixA;
                        if (   (m_tokens.size() > 0)
                            && (m_tokens.at(0).name.compare(
                                    "EXISTS", Qt::CaseInsensitive) == 0))
                        { // valid prefix
                            m_tokens.removeFirst();
                            expr->terminal.name = "NOT EXISTS";
                        } else { // valid prefix
                            expr->terminal.name = "NOT";
                        } // NOT at end of expression isn't valid
                        // but we'll catch that when we look for the next token
                        expr->right = internalParser(1, level, ends);
                        if (expr->right == NULL) { // invalid expression
                            destroyExpression(expr);
                            return NULL;
                        } else { // recursive call found rest of expression
                            return expr; // done at this level
                        }
                    default: // expected operand, got something else
                        return NULL;
                }
                state = 3;
                /*FALLTHRU*/
            case 3: // had an operand, expecting operator or end
                    // ',' is a valid operator at level 1
                    // expr has expression so far
                    // returns entire expression at this recursion depth
                if (m_tokens.isEmpty()) { // end after operand
                    if (level == 0) { // end after operand is valid at level 0
                        return expr;
                    } else { // unmatched '('
                        destroyExpression(expr);
                        return NULL;
                    }
                } else if (   ((level == 0) || (level == 3))
                           && (m_tokens.at(0).type != tokenQuotedIdentifier)
                           && (m_tokens.at(0).type != tokenSquareIdentifier)
                           && (m_tokens.at(0).type !=
                                                tokenBackQuotedIdentifier)
                           && (m_tokens.at(0).type != tokenStringLiteral)
                           && ends.contains(
                                m_tokens.at(0).name, Qt::CaseInsensitive))
                { return expr; } // end after operand is valid at level 0
                t = m_tokens.takeFirst(); // m_tokens isn't empty now
                switch (t.type) {
                    case tokenPostfixA:
                    {
                        Expression * e = new Expression();
                        e->type = exprXOp;
                        e->left = expr;
                        e->terminal = t;
                        expr = e;
                        continue; // state still 3
                    }
                    case tokenComma:
                        if (level == 1) { // ',' valid in arglist
                            Expression * e = new Expression;
                            e->type = exprXOpX;
                            e->left = expr;
                            e->terminal = t;
                            e->right = internalParser(1, 1, ends);
                            if (e->right == NULL) { // invalid expression
                                destroyExpression(expr);
                                return NULL;
                            } else { return e; } // done at this level
                        } else { // ',' not valid elsewhere
                            destroyExpression(expr);
                            return NULL;
                        }
                    case tokenClose:
                        if (level == 0) { // unmatched ')'
                            destroyExpression(expr);
                            return NULL;
                        } else { return expr; } // ')' OK in levels 1 and 2
                    case tokenOperatorA:
                        if (   (t.name.compare("IS") == 0)
                            && (m_tokens.size() > 0)
                            && (m_tokens.at(0).type == tokenNOT)) {
                            // treat IS NOT as a single operator
                            m_tokens.removeFirst();
                            t.name = "IS NOT";
                        }
                        /*FALLTHRU*/
                    case tokenOperator:
                    case tokenPlusMinus:
                    {
                        Expression * e = new Expression();
                        e->left = expr;
                        e->type = exprXOpX;
                        e->terminal = t;
                        e->right = internalParser(1, level, ends);
                        if (e->right == 0) { // invalid expression
                            destroyExpression(e);
                            return 0;
                        } else { return e; } // done at this level
                    }
                    case tokenNOT:
                        if (m_tokens.size() > 0) {
                            QString n(m_tokens.at(0).name);
                            if (n.compare( "BETWEEN", Qt::CaseInsensitive)
                                == 0)
                            {
                                m_tokens.removeFirst();
                                Expression * e = new Expression();
                                e->left = expr;
                                e->type = exprXOpX;
                                e->terminal.name = "NOT BETWEEN";
                                e->terminal.type = tokenOperatorA;
                                e->right = internalParser(1, level, ends);
                                if (e->right == 0) { // invalid expression
                                    destroyExpression(e);
                                    return 0;
                                } else { return e; } // done at this level
                            }
                            if (n.compare( "NULL", Qt::CaseInsensitive) == 0) {
                                // NOT NULL is a postfix operator
                                m_tokens.removeFirst();
                                Expression * e = new Expression();
                                e->type = exprXOp;
                                e->terminal.name = "NOT NULL";
                                e->terminal.type = tokenPostfixA;
                                e->left = expr;
                                expr = e;
                                continue; // state still 3
                            } else if (afterNOTs.contains(
                                n, Qt::CaseInsensitive))
                            {
                                m_tokens.removeFirst();
                                Expression * e = new Expression();
                                e->left = expr;
                                e->type = exprXOpX;
                                e->terminal.name = n.prepend("NOT ");
                                e->terminal.type = tokenPostfixA;
                                e->right = internalParser(1, level, ends);
                                if (e->right == 0) { // invalid expression
                                    destroyExpression(e);
                                    return 0;
                                } else { return e; } // done at this level
                            }
                            // NOT by itself isn't a valid binary operator
                        }
                        // NOT by itself isn't valid at end of expression
                        destroyExpression(expr);
                        return 0;
                    default: // invalid expression, probably missing operator
                        destroyExpression(expr);
                        return 0;
                }
            case 4: // CAST expression, had '(', look for expression
                expr = new Expression;
                expr->type = exprXOpX;
                expr->left = internalParser(1, 3, QStringList({"AS"}));
                if ((expr == NULL) || m_tokens.isEmpty()) {
                    expr->right = NULL;
                    destroyExpression(expr);
                    return NULL;
                }
                m_tokens.removeFirst(); // remove the AS
                expr->terminal.type = tokenOperatorA;
                expr->terminal.name = "AS";
                expr->right = internalParser(5, 3, QStringList());
                if (expr->right == NULL) {
                    destroyExpression(expr);
                    return NULL;
                } else { return expr; }
            case 5: // CAST expression, had AS, looking for type-name
                expr = new Expression;
                expr->type = exprToken;
                expr->terminal.type = tokenIdentifier;
                expr->terminal.name = "";
                while (1) { // accumulate parts of type name
                    if (m_tokens.isEmpty()) {
                        destroyExpression(expr);
                        return NULL;
                    }
                    t = m_tokens.takeFirst();
                    if (!expr->terminal.name.isEmpty()) { // seen >=1 name
                        if (t.type == tokenClose) { return expr; }
                        else if (t.type == tokenOpen) { break; }
                    }
                    if (   (t.type != tokenQuotedIdentifier)
                        && (t.type != tokenSquareIdentifier)
                        && (t.type != tokenBackQuotedIdentifier)
                        && (t.type != tokenStringLiteral)
                        && (t.type != tokenIdentifier))
                    {
                        destroyExpression(expr);
                        return NULL;
                    }
                    if (expr->terminal.name.isEmpty()) {
                        expr->terminal.name = t.name;
                    } else {
                        expr->terminal.name.append(" ").append(t.name);
                    }
                }
                expr->type = exprCall; // seen '(' after type name
                // Sqlite ignores field widths,
                // but we still have to parse them.
                expr->right = internalParser(6, 3, QStringList());
                if (expr->right == NULL) {
                    destroyExpression(expr);
                    return NULL; // bad or unterminated field width
                }
                t = m_tokens.takeFirst();
                if (t.type == tokenComma) {
                    Expression * e = new Expression;
                    e->type = exprXOpX;
                    e->left = expr->right;
                    e->terminal = t;
                    expr->right = e;
                    e->right = internalParser(6, 3, QStringList());
                    if (e->right == NULL) {
                        destroyExpression(expr);
                        return NULL; // bad or unterminated field width
                    }
                    t = m_tokens.takeFirst();
                }
                if ((t.type != tokenClose) || m_tokens.isEmpty()) {
                    destroyExpression(expr);
                    return NULL; // field width not terminated by ')'
                }
                t = m_tokens.takeFirst(); // CAST not terminated by ')'
                if (t.type != tokenClose) {
                    destroyExpression(expr);
                    return NULL; // CAST expression not terminated by ')'
                }
                return expr; // done at this recursion depth
            case 6: // get a signed number, does not absorb terminator
                if (m_tokens.isEmpty()) {return NULL; } // premature end
                t = m_tokens.takeFirst();
                if (t.type == tokenPlusMinus) { // optional sign
                    if (m_tokens.isEmpty()) { return NULL; }
                    expr = new Expression;
                    expr->type = exprOpX;
                    expr->terminal = t;
                    t = m_tokens.takeFirst();
                    if ((t.type != tokenNumeric) || m_tokens.isEmpty()) {
                        delete expr; // bad token or no more
                        return NULL;
                    }
                    expr->right = new Expression;
                    expr->right->type = exprToken;
                    expr->right->terminal = t;
                    return expr;
                } else if ((t.type != tokenNumeric) || m_tokens.isEmpty()) {
                    return NULL;
                }
                expr = new Expression;
                expr->type = exprToken;
                expr->terminal = t;
                return expr; // done at this recursion level
       }
    }
}

// Parse expression from "input".
// Returns NULL if the expression is invalid
// or an Expression which is the root of the parse tree.
// This is a special case for parsing a DEFAULT value in TableEditorDialog.
Expression * SqlParser::parseExpression(QString input) {
    m_tokens = tokenise(input);
    return internalParser(0, 0, QStringList());
}

void SqlParser::clearField(FieldInfo &f)
{
	f.name = QString();
	f.type = QString();
	f.collator = QString();
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
    f.isUnique = false;
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

// Currently this only parses CREATE TABLE or CREATE INDEX statements
// (which sqliteman can alter) as they can appear in the schema.
SqlParser::SqlParser(QString input)
{
	m_isUnique = false;
	m_hasRowid = true;
	m_whereClause = NULL;
	m_tokens = tokenise(input);
	m_depth = 0;
	enum sqlParserState state = expectCREATE; // nothing
	FieldInfo f;
	clearField(f);
	while (!m_tokens.isEmpty())
	{
        Token t = m_tokens.at(0);
		QString s;
        if (t.type == tokenWhitespace) { continue; }
        else if (t.type == tokenIdentifier) { s = t.name; } else { s = ""; }
		switch (state)
		{
			case expectCREATE: // we only parse CREATE statements so far
				if (s.compare("CREATE", Qt::CaseInsensitive) == 0) {
					state = expectCreated; // seen CREATE
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case expectCreated: // seen CREATE
				// TEMP[ORARY] doesn't get copied to schema
                if (s.compare("TABLE", Qt::CaseInsensitive) == 0) {
                    state = TexpectTableName; // CREATE TABLE
                    m_type = createTable;
                } else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    m_isUnique = true;
                    state = IexpectINDEX; // CREATE UNIQUE
                } else if (s.compare("INDEX", Qt::CaseInsensitive) == 0) {
                    state = IexpectIndexName; // CREATE INDEX
                } else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TexpectTableName: // CREATE TABLE
				// IF NOT EXISTS doesn't get copied to schema
				// schema-name "." doesn't get copied to schema
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					m_tableName = t.name;
					state = TexpectColumnList; // had table name
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TexpectColumnList:
				if (t.type == tokenOpen) {
					state = TexpectColumnName; // CREATE TABLE ( <columns...>
				} else { break; } // AS SELECT is not copied to the schema
				m_tokens.removeFirst();
				continue;
			case TexpectColumnName: // look for a column definition
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					f.name = t.name;
					state = TexpectColumnType; // look for type
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
            // after ',', expect column name or table constraint
			case TexpectColOrTC:
				if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0) {
					state = TCexpectName; // look for table constraint name
				} else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = TCexpectPKEY; // look for KEY
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = TCexpectUOpen; // look for columns and conflict clause
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = TCexpectCheckOpen; // look for bracketed expression
				} else if (s.compare("FOREIGN", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKEY; // look for KEY
				} else if (   (t.type == tokenIdentifier)
                           || (t.type == tokenQuotedIdentifier)
                           || (t.type == tokenSquareIdentifier)
                           || (t.type == tokenBackQuotedIdentifier)
                           || (t.type == tokenStringLiteral))
				{
					f.name = t.name;
					state = TexpectColumnType;
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
                // after column name expect type or column constraint or , or )
			case TexpectColumnType:
                // look for type or column constraint or , or )
				if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
					state = TexpectColOrTC;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0) {
					state = CCexpectName; // look for column constraint name
				} else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = CCexpectKEY; // look for KEY
				} else if (t.type == tokenNOT) {
					state = CCexpectNULL; // look for NULL
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else if (   (t.type == tokenIdentifier)
						   || (t.type == tokenQuotedIdentifier)
						   || (t.type == tokenSquareIdentifier)
						   || (t.type == tokenBackQuotedIdentifier)
						   || (t.type == tokenStringLiteral))
				{ // look for more type name or column constraint or , or ) 
                    f.type = t.name;
                    state = TexpectTypeQualifier;
                } else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
                // look for more type name or column constraint or , or )
			case TexpectTypeQualifier:
				if (t.type == tokenOpen) {
                    f.type.append("(");
					state = TexpectFieldWidth1; // look for field width(s)
				} else if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
					state = TexpectColOrTC;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0) {
					state = CCexpectName; // look for column constraint name
				} else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = CCexpectKEY; // look for KEY
				} else if (t.type == tokenNOT) {
					state = CCexpectNULL; // look for NULL
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else if (   (t.type == tokenIdentifier)
						   || (t.type == tokenQuotedIdentifier)
						   || (t.type == tokenSquareIdentifier)
						   || (t.type == tokenBackQuotedIdentifier)
						   || (t.type == tokenStringLiteral))
				{ f.type.append(" ").append(t.name); }
                else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
            case TexpectFieldWidth1: // look for field width(s)
                if (t.type == tokenPlusMinus) {
                    f.type.append(t.name);
                    state = TexpectFieldWidth1N; // only one sign allowed before number
                } else if (t.type == tokenNumeric) { // real IS allowed
                    f.type.append(t.name);
                    state = TexpectFieldWidthSep; // look for , or )
                } else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
            case TexpectFieldWidth1N: // look for first field width after sign
                if (t.type == tokenNumeric) { // real IS allowed
                    f.type.append(t.name);
                    state = TexpectFieldWidthSep; // look for , or )
                } else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
            case TexpectFieldWidthSep: // look for second field width or )
                if (t.type == tokenComma) {
                    f.type.append(t.name);
                    state = TexpectFieldWidth2; // look for second field width
                } else if (t.type == tokenClose) {
                    f.type.append(t.name);
                    state = TexpectColConstraint;
                } else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
            case TexpectFieldWidth2: // look for second field width
                if (t.type == tokenPlusMinus) {
                    f.type.append(t.name);
                    state = TexpectFieldWidth2N; // only one sign allowed before number
                } else if (t.type == tokenNumeric) { // real IS allowed
                    f.type.append(t.name);
                    state = TexpectFieldWidthClose; // look for )
                } else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
            case TexpectFieldWidth2N: // look for second field width after sign
                if (t.type == tokenNumeric) { // real IS allowed
                    f.type.append(t.name);
                    state = TexpectFieldWidthClose; // look for )
                } else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
            case TexpectFieldWidthClose: // look for ) after second field width
                if (t.type == tokenClose) {
                    f.type.append(t.name);
                    state = TexpectColConstraint; // look for column constraint or , or )
                } else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TexpectColConstraint: // look for column constraint or , or )
				if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
                    // look for column definition or table constraint
					state = TexpectColOrTC;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // expect WITHOUT ROWID or end
				} else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0) {
					state = CCexpectName; // look for column constraint name
				} else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = CCexpectKEY; // look for KEY
				} else if (t.type == tokenNOT) {
					state = CCexpectNULL; // look for NULL
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectName: // look for column constraint name
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					state = CCexpectType; // look for constraint or , or )
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectType: // look for constraint
				if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = CCexpectKEY; // look for KEY
				} else if (t.type == tokenNOT) {
					state = CCexpectNULL; // look for NULL
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectKEY: // look for KEY
				if (s.compare("KEY", Qt::CaseInsensitive) == 0) {
					addToPrimaryKey(f);
					state = CCexpectPKqualifier; // look for ASC or DESC or conflict clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectPKqualifier: // look for ASC or DESC or conflict clause
				if (s.compare("ASC", Qt::CaseInsensitive) == 0) {
					state = CCexpectPKConflict; // look for conflict clause
				} else if (s.compare("DESC", Qt::CaseInsensitive) == 0) {
					f.isColumnPkDesc = true;
					state = CCexpectPKConflict; // look for conflict clause
				} else if (s.compare("ON", Qt::CaseInsensitive) == 0) {
					state = CCexpectPKCONFLICT; // look for CONFLICT
				} else if (s.compare(
                    "AUTOINCREMENT", Qt::CaseInsensitive) == 0)
				{
					f.isAutoIncrement = true;
					state = TexpectColConstraint; // look for column constraint or , or )
				} else if (t.type == tokenNOT) {
					state = CCexpectNULL; // look for NULL
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
					state = TexpectColumnName;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectPKConflict: // look for conflict clause
				if (s.compare("ON", Qt::CaseInsensitive) == 0) {
					state = CCexpectPKCONFLICT; // look for CONFLICT
				} else if (s.compare(
                    "AUTOINCREMENT", Qt::CaseInsensitive) == 0)
				{
                    if (f.isColumnPkDesc) { break; } // not after DESC
					f.isAutoIncrement = true;
					state = TexpectColConstraint; // look for column constraint or , or )
				} else if (t.type == tokenNOT) {
					state = CCexpectNULL; // look for NULL
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
					state = TexpectColumnName;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectPKCONFLICT: // look for CONFLICT
				if (s.compare("CONFLICT", Qt::CaseInsensitive) == 0) {
					state = CCexpectPKConflictAction; // look for conflict action
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectPKConflictAction: // look for conflict action
				if (s.compare("ROLLBACK", Qt::CaseInsensitive) == 0) {
					state = CCexpectAUTO; // look for AUTOINCREMENT or next
				} else if (s.compare("ABORT", Qt::CaseInsensitive) == 0) {
					state = CCexpectAUTO; // look for AUTOINCREMENT or next
				} else if (s.compare("FAIL", Qt::CaseInsensitive) == 0) {
					state = CCexpectAUTO; // look for AUTOINCREMENT or next
				} else if (s.compare("IGNORE", Qt::CaseInsensitive) == 0) {
					state = CCexpectAUTO; // look for AUTOINCREMENT or next
				} else if (s.compare("REPLACE", Qt::CaseInsensitive) == 0) {
					state = CCexpectAUTO; // look for AUTOINCREMENT or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectAUTO: // look for AUTOINCREMENT or next
				if (s.compare("AUTOINCREMENT", Qt::CaseInsensitive) == 0) {
					//FIXME DESC or ASC is allowed after AUTOINCREMENT
					f.isAutoIncrement = true;
					state = TexpectColConstraint; // look for column constraint or , or )
				} else if (t.type == tokenNOT) {
					state = CCexpectNULL; // look for NULL
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
					state = TexpectColOrTC;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectNULL: // look for NULL
				if (s.compare("NULL", Qt::CaseInsensitive) == 0) {
					f.isNotNull = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectConflict: // look for conflict clause or next
				if (s.compare("ON", Qt::CaseInsensitive) == 0) {
					state = CCexpectCONFLICT; // look for CONFLICT
				} else if (t.type == tokenNOT) {
					state = CCexpectNULL; // look for NULL
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
					state = TexpectColumnName;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectCONFLICT: // look for CONFLICT
				if (s.compare("CONFLICT", Qt::CaseInsensitive) == 0) {
					state = CCexpectConflictAction; // look for conflict action
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectConflictAction: // look for conflict action
				if (s.compare("ROLLBACK", Qt::CaseInsensitive) == 0) {
					state = TexpectColConstraint; // look for constraint or , or )
				} else if (s.compare("ABORT", Qt::CaseInsensitive) == 0) {
					state = TexpectColConstraint; // look for constraint or , or )
				} else if (s.compare("FAIL", Qt::CaseInsensitive) == 0) {
					state = TexpectColConstraint; // look for constraint or , or )
				} else if (s.compare("IGNORE", Qt::CaseInsensitive) == 0) {
					state = TexpectColConstraint; // look for constraint or , or )
				} else if (s.compare("REPLACE", Qt::CaseInsensitive) == 0) {
					state = TexpectColConstraint; // look for constraint or , or )
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectOpenExprClose: // look for bracketed expression
				if (t.type == tokenOpen) {
					state = CCexpectExprClose; // look for end of expression
					++m_depth;
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectExprClose: // look for end of expression
				if (t.type == tokenOpen) { ++m_depth; }
				else if (t.type == tokenClose) {
					if (--m_depth == 0) { state = TexpectColConstraint; }
				}
				m_tokens.removeFirst();
				continue;
			case CCexpectDefault: // look for default value
                if (t.type == tokenOpen) {
					f.defaultIsExpression = true;
					f.defaultValue = "(";
                    m_lastToString = t;
					state = CCexpectDefaultExprClose; // scan for end of default value expression
					++m_depth;
				} else if (t.type == tokenPlusMinus) {
					f.defaultValue = t.name;
					state = CCexpectDefaultLiteral; // look for (signed) number
				} else {
					if (   (t.type == tokenStringLiteral)
                        || (t.type == tokenQuotedIdentifier)
						|| (t.type == tokenSquareIdentifier)
						|| (t.type == tokenBackQuotedIdentifier))
					{ f.defaultisQuoted = true; }
					f.defaultValue = s;
					state = TexpectColConstraint; // look for column constraint or , or )
				}
				m_tokens.removeFirst();
				continue;
			case CCexpectDefaultExprClose: // scan for end of default value expression
                // We don't attempt to parse it because we got it
                // from the schema so we know it is valid.
				if (t.type == tokenOpen) {
					f.defaultValue.append("(");
					++m_depth;
				} else if (t.type == tokenClose) {
					f.defaultValue.append(")");
					if (--m_depth == 0) { state = TexpectColConstraint; }
				} else {
					f.defaultValue.append(tos(t));
				}
				m_tokens.removeFirst();
				continue;
			case CCexpectDefaultLiteral: // look for (signed) number
                // Strangely sqlite allows a string literal instead
				if (   (t.type == tokenNumeric)
                    || (t.type == tokenStringLiteral))
				{
					f.defaultValue.append(t.name);
					state = TexpectColConstraint; // look for column constraint or , or )
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectCollateName: // look for collation name
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
                    f.collator = t.name;
					state = TexpectColConstraint; // look for constraint or , or )
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectREFTable: // look for (foreign) table name
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
                    f.referencedTable = t.name;
					state = CCexpectREFOpen; // look for clause or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
            // look for column list or rest of foreign key clause
			case CCexpectREFOpen:
				if (t.type == tokenOpen) {
					state = CCexpectREFColumn; // look for column list
				} else if (s.compare("ON", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKDeleteUpdate; // look for DELETE or UPDATE
				} else if (s.compare("MATCH", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKMatchType; // look for SIMPLE or PARTIAL or FULL
				} else if (t.type == tokenNOT) {
					state = CCexpectFKDEFERRABLE; // look for DEFERRABLE or NULL
				} else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKINITIALLY; // look for INITIALLY or next
				} else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0) {
					state = CCexpectName; // look for column constraint name
				} else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = CCexpectKEY; // look for KEY
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
					state = TexpectColumnName;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				}
				else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectREFColumn: // look for list with only one column name
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
                    f.referencedKeys.append(t.name);
					state = CCexpectREFClose; // look for next column name or )
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectREFClose: // look for next column name or )
				if (t.type == tokenClose) {
					state = CCexpectFKType; // look for rest of foreign key clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectFKType: // look for rest of foreign key clause
				if (s.compare("ON", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKDeleteUpdate; // look for DELETE or UPDATE
				} else if (s.compare("MATCH", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKMatchType; // look for SIMPLE or PARTIAL or FULL
				} else if (t.type == tokenNOT) {
					state = CCexpectFKDEFERRABLE; // look for DEFERRABLE or NULL
				} else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKINITIALLY; // look for INITIALLY
				} else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0) {
					state = CCexpectName; // look for column constraint name
				} else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = CCexpectKEY; // look for KEY
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
					state = TexpectColumnName;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectFKDeleteUpdate: // look for DELETE or UPDATE
				if (s.compare("DELETE", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKDUAction; // look for foreign key action
				} else if (s.compare("UPDATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKDUAction; // look for foreign key action
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectFKDUAction: // look for foreign key DELETE / UPDATE action
				if (s.compare("SET", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKSetAction; // look for NULL or DEFAULT
				} else if (s.compare("CASCADE", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("RESTRICT", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("NO", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKACTION; // look for ACTION
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectFKSetAction: // look for NULL or DEFAULT
				if (s.compare("NULL", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKType; // look for rest of foreign key clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectFKACTION: // look for ACTION
				if (s.compare("ACTION", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKType; // look for rest of foreign key clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectFKMatchType: // look for SIMPLE or PARTIAL or FULL
				if (s.compare("SIMPLE", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("PARTIAL", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("FULL", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKType; // look for rest of foreign key clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectFKDEFERRABLE: // look for DEFERRABLE or NULL
				if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKINITIALLY; // look for INITIALLY etc
				} else if (s.compare("NULL", Qt::CaseInsensitive) == 0) {
					f.isNotNull = true;
					state = CCexpectAUTO; // look for conflict clause or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case CCexpectFKINITIALLY: // look for INITIALLY or next
				if (s.compare("INITIALLY", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKDeferType; // look for DEFERRED or IMMEDIATE
				} else if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0) {
					state = CCexpectName; // look for column constraint name
				} else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = CCexpectKEY; // look for KEY
				} else if (t.type == tokenNOT) {
					state = CCexpectNULL; // look for NULL
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = CCexpectConflict; // look for conflict clause or next
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = CCexpectOpenExprClose; // look for bracketed expression
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = CCexpectDefault; // look for default value
				} else if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = CCexpectCollateName; // look for collation name
				} else if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = CCexpectREFTable; // look for (foreign) table name
				} else if (t.type == tokenComma) {
					m_fields.append(f);
					clearField(f);
					state = TexpectColumnName;
				} else if (t.type == tokenClose) {
					m_fields.append(f);
					clearField(f);
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case 45: // look for DEFERRED or IMMEDIATE
				if (s.compare("DEFERRED", Qt::CaseInsensitive) == 0) {
					state = TexpectColConstraint; // look for column constraint or , or )
				} else if (s.compare("IMMEDIATE", Qt::CaseInsensitive) == 0) {
					state = TexpectColConstraint; // look for column constraint or , or )
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectName: // look for table constraint name
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					state = TCexpectType; // look for rest of table constraint
				}
				else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectType: // look for rest of table constraint
				if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = TCexpectPKEY; // look for KEY
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = TCexpectUOpen; // look for columns and conflict clause
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = TCexpectCheckOpen; // look for bracketed expression
				} else if (s.compare("FOREIGN", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKEY; // look for KEY
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectPKEY: // look for KEY
				if (s.compare("KEY", Qt::CaseInsensitive) == 0) {
					state = TCexpectPKOpen; // look for columns and conflict clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectPKOpen: // look for columns and conflict clause
				if (t.type == tokenOpen) { state = TCexpectPKColumn; }
				else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectPKColumn: // look for next column in list
				// sqlite doesn't currently support expressions here
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					addToPrimaryKey(s);
					state = TCexpectPKQualifier; // look for COLLATE or ASC/DESC or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectPKQualifier: // look for COLLATE or ASC/DESC or next
				if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = TCexpectPKCollateName; // look for collation name
				} else if (s.compare("ASC", Qt::CaseInsensitive) == 0) {
					state = TCexpectPKNext; // look for , or )
				} else if (s.compare("DESC", Qt::CaseInsensitive) == 0) {
					f.isTablePkDesc = true;
					state = TCexpectPKNext; // look for , or )
				} else if (t.type == tokenComma) {
					state = TCexpectPKColumn; // look for next column in list
				} else if (t.type == tokenClose) {
					state = TCexpectONorNext; // look for conflict clause or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectPKCollateName: // look for collation name
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					state = TCexpectPKAscDesc; // look for ASC/DESC or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectPKAscDesc: // look for ASC/DESC or next
				if (s.compare("ASC", Qt::CaseInsensitive) == 0) {
					state = TCexpectPKNext; // look for , or )
				} else if (s.compare("DESC", Qt::CaseInsensitive) == 0) {
					state = TCexpectPKNext; // look for , or )
				} else if (t.type == tokenComma) {
					state = TCexpectPKColumn; // look for next column in list
				} else if (t.type == tokenClose) {
					state = TCexpectONorNext; // look for conflict clause or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectPKNext: // look for , or )
				if (t.type == tokenComma) {
					state = TCexpectPKColumn; // look for next column in list
				} else if (t.type == tokenClose) {
					state = TCexpectONorNext; // look for conflict clause or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectUOpen: // look for columns and conflict clause
				if (t.type == tokenOpen) {
					state = TCexpectUColumn;
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectUColumn: // look for next column in list
				// sqlite doesn't currently support expressions here
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{ // look for COLLATE or ASC/DESC or next
					state = TCexpectUQualifier;
				}  else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectUQualifier: // look for COLLATE or ASC/DESC or next
				if (s.compare("COLLATE", Qt::CaseInsensitive) == 0) {
					state = TCexpectUCollateName; // look for collation name
				} else if (s.compare("ASC", Qt::CaseInsensitive) == 0) {
					state = TCexpectUNext; // look for , or )
				} else if (s.compare("DESC", Qt::CaseInsensitive) == 0) {
					state = TCexpectUNext; // look for , or )
				} else if (t.type == tokenComma) {
					state = TCexpectUColumn; // look for next column in list
				} else if (t.type == tokenClose) {
					state = TCexpectONorNext; // look for conflict clause or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectUCollateName: // look for collation name
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					state = TCexpectUAscDesc; // look for ASC/DESC or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectUAscDesc: // look for ASC/DESC or next
				if (s.compare("ASC", Qt::CaseInsensitive) == 0) {
					state = TCexpectUNext; // look for , or )
				} else if (s.compare("DESC", Qt::CaseInsensitive) == 0) {
					state = TCexpectUNext; // look for , or )
				} else if (t.type == tokenComma) {
					state = TCexpectUColumn; // look for next column in list
				} else if (t.type == tokenClose) {
					state = TCexpectONorNext; // look for conflict clause or next
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectUNext: // look for , or )
				if (t.type == tokenComma) {
					state = TCexpectUColumn; // look for next column in list
				} else if (t.type == tokenClose) {
					state = TCexpectONorNext; // look for conflict clause or next
				}  else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectONorNext: // look for conflict clause or next
				if (s.compare("ON", Qt::CaseInsensitive) == 0) {
					state = TCexpectCONFLICT; // look for CONFLICT
				} else if (t.type == tokenComma) {
					state = TexpectTableConstraint; // look for next table constraint
				} else if (t.type == tokenClose) {
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectCONFLICT: // look for CONFLICT
				if (s.compare("CONFLICT", Qt::CaseInsensitive) == 0) {
					state = TCexpectConflictAction; // look for conflict action
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectConflictAction: // look for conflict action
				if (s.compare("ROLLBACK", Qt::CaseInsensitive) == 0) {
					state = TCexpectAnotherOrEnd; // look for next table constraint or end
				} else if (s.compare("ABORT", Qt::CaseInsensitive) == 0) {
					state = TCexpectAnotherOrEnd; // look for next table constraint or end
				} else if (s.compare("FAIL", Qt::CaseInsensitive) == 0) {
					state = TCexpectAnotherOrEnd; // look for next table constraint or end
				} else if (s.compare("IGNORE", Qt::CaseInsensitive) == 0) {
					state = TCexpectAnotherOrEnd; // look for next table constraint or end
				} else if (s.compare("REPLACE", Qt::CaseInsensitive) == 0) {
					state = TCexpectAnotherOrEnd; // look for next table constraint or end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectCheckOpen: // look for bracketed expression
				if (t.type == tokenOpen) {
					state = TCexpectCheckExpr; // look for end of expression
					++m_depth;
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectCheckExpr: // look for end of expression
				if (t.type == tokenOpen) {
					++m_depth;
				} else if (t.type == tokenClose) {
					if (--m_depth == 0) { state = TCexpectAnotherOrEnd; }
				}
				m_tokens.removeFirst();
				continue;
			case TCexpectFKEY: // look for KEY
				if (s.compare("KEY", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKOpen; // look for column list
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKOpen: // look for column list
				if (t.type == tokenOpen) { state = TCexpectFKColumn; }
				else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKColumn: // look for next column in list
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					state = TCexpectFKClose; // look for , or )
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKClose: // look for , or )
				if (s.compare(",") == tokenComma) {
					state = TCexpectFKColumn; // look for next column in list
				} else if (s.compare(")") == tokenClose) {
					state = TCexpectREFERENCES; // look for foreign key clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectREFERENCES: // look for foreign key clause
				if (s.compare("REFERENCES", Qt::CaseInsensitive) == 0) {
					state = TCexpectREFTable; // look for table name
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectREFTable: // look for table name
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					state = TCexpectREFOpen; // look for column list or rest of clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectREFOpen: // look for column list or rest of clause
				if (t.type == tokenOpen) {
					state = TCexpectREFColumn; // look for next column in list
				} else if (s.compare("ON", Qt::CaseInsensitive) == 0) {
					state = CCexpectFKDeleteUpdate; // look for DELETE or UPDATE
				} else if (s.compare("MATCH", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKMatchType; // look for SIMPLE or PARTIAL or FULL
				} else if (t.type == tokenNOT) {
					state = TCexpectFKDEFERRABLE; // look for DEFERRABLE
				} else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKINITIALLY; // look for INITIALLY
				} else if (t.type == tokenComma) {
					state = TexpectTableConstraint; // look for next table constraint
				} else if (t.type == tokenClose) {
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectREFColumn: // look for next column in list
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					state = TCexpectREFClose; // look for , or )
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectREFClose:
				if (t.type == tokenComma) {
					state = TCexpectREFColumn; // look for next column in list
				} else if (t.type == tokenClose) {
					state = TCexpectFKType; // look for rest of foreign key clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKType: // look for rest of foreign key clause
				if (s.compare("ON", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKDeleteUpdate; // look for DELETE or UPDATE
				} else if (s.compare("MATCH", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKMatchType; // look for SIMPLE or PARTIAL or FULL
				} else if (t.type == tokenNOT) {
					state = TCexpectFKDEFERRABLE; // look for DEFERRABLE
				} else if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKINITIALLY; // look for INITIALLY
				} else if (t.type == tokenComma) {
					state = TexpectTableConstraint; // look for next table constraint
				} else if (t.type == tokenClose) {
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKDeleteUpdate: // look for DELETE or UPDATE
				if (s.compare("DELETE", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKDUAction; // look for foreign key action
				} else if (s.compare("UPDATE", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKDUAction; // look for foreign key action
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKDUAction: // look for foreign key action
				if (s.compare("SET", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKSetAction; // look for NULL or DEFAULT
				} else if (s.compare("CASCADE", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("RESTRICT", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("NO", Qt::CaseInsensitive) == 0) {
					state = TCexpectACTION; // look for ACTION
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKSetAction: // look for NULL or DEFAULT
				if (s.compare("NULL", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("DEFAULT", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKType; // look for rest of foreign key clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectACTION: // look for ACTION
				if (s.compare("ACTION", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKType; // look for rest of foreign key clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKMatchType: // look for SIMPLE or PARTIAL or FULL
				if (s.compare("SIMPLE", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("PARTIAL", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKType; // look for rest of foreign key clause
				} else if (s.compare("FULL", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKType; // look for rest of foreign key clause
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKDEFERRABLE: // look for DEFERRABLE
				if (s.compare("DEFERRABLE", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKINITIALLY; // look for INITIALLY etc
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKINITIALLY: // look for INITIALLY or next
				if (s.compare("INITIALLY", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKDeferType; // look for DEFERRED or IMMEDIATE
				} else if (t.type == tokenComma) {
					state = TexpectTableConstraint; // look for next table constraint
				} else if (t.type == tokenClose) {
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectFKDeferType: // look for DEFERRED or IMMEDIATE
				if (s.compare("DEFERRED", Qt::CaseInsensitive) == 0) {
					state = TCexpectAnotherOrEnd; // look for next table constraint or end
				} else if (s.compare("IMMEDIATE", Qt::CaseInsensitive) == 0) {
					state = TCexpectAnotherOrEnd; // look for next table constraint or end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TCexpectAnotherOrEnd: // look for next table constraint or end
				if (t.type == tokenComma) {
					state = TexpectTableConstraint; // look for next table constraint
				} else if (t.type == tokenClose) {
					state = TexpectWITHOUT; // check for WITHOUT ROWID or rubbish at end
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TexpectTableConstraint: // look for next table constraint
				if (s.compare("CONSTRAINT", Qt::CaseInsensitive) == 0) {
					state = TCexpectName; // look for table constraint name
				} else if (s.compare("PRIMARY", Qt::CaseInsensitive) == 0) {
					state = TCexpectPKEY; // look for KEY
				} else if (s.compare("UNIQUE", Qt::CaseInsensitive) == 0) {
                    f.isUnique = true;
					state = TCexpectUOpen; // look for columns and conflict clause
				} else if (s.compare("CHECK", Qt::CaseInsensitive) == 0) {
					state = TCexpectCheckOpen; // look for bracketed expression
				} else if (s.compare("FOREIGN", Qt::CaseInsensitive) == 0) {
					state = TCexpectFKEY; // look for KEY
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TexpectWITHOUT: // check for WITHOUT ROWID or rubbish at end
				if (s.compare("WITHOUT", Qt::CaseInsensitive) == 0) {
					state = TexpectROWID; // seen WITHOUT
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case TexpectROWID: // seen WITHOUT
				if (s.compare("ROWID", Qt::CaseInsensitive) == 0) {
					state = expectStatementEnd; // seen ROWID
					m_hasRowid = false;
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case IexpectINDEX: // CREATE UNIQUE
				if (s.compare("INDEX", Qt::CaseInsensitive) == 0) {
					state = IexpectIndexName; // CREATE UNIQUE INDEX
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case IexpectIndexName: // CREATE [UNIQUE] INDEX
				// IF NOT EXISTS doesn't get copied to schema
				// schema-name "." doesn't get copied to schema
				m_type = createIndex;
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					m_indexName = s;
					state = IexpectON; // had index name
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case IexpectON: // look for ON
				if (s.compare("ON", Qt::CaseInsensitive) == 0) {
					state = IexpectTableName; // look for table name
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case IexpectTableName: // look for table name
				if (   (t.type == tokenIdentifier)
					|| (t.type == tokenQuotedIdentifier)
					|| (t.type == tokenSquareIdentifier)
					|| (t.type == tokenBackQuotedIdentifier)
					|| (t.type == tokenStringLiteral))
				{
					m_tableName = s;
					state = IexpectColumnList; // look for indexed column list
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case IexpectColumnList: // look for indexed column list
				if (t.type == tokenOpen) {
					state = IexpectIndexedColumn; // look for indexed column
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case IexpectIndexedColumn: // look for indexed column
			{ // look for arglist which is near enough
				Expression * expr = internalParser(0, 1, QStringList());
				if (   (expr == NULL) // invalid expression
                    || (expr->type == exprNull)) // empty column list
                { break ; } // not a valid create statement
				while (   (expr->type == exprXOpX)
                       && (expr->terminal.type == tokenComma))
                { // take one indexed column from the expression
                    m_columns.append(expr->left);
                    expr = expr->right;
                }
                m_columns.append(expr); // last column terminated by ')'
                state = IexpectWHERE; // look for end or WHERE clause
				continue;
			}
			case IexpectWHERE: // look for end or WHERE clause
				if (s.compare("WHERE", Qt::CaseInsensitive) == 0) {
					state = IexpectWhereExpression; // look for expression
				} else { break; } // not a valid create statement
				m_tokens.removeFirst();
				continue;
			case IexpectWhereExpression: // look for expression
				m_whereClause = internalParser(0, 0, QStringList());
				if (m_whereClause != NULL) {
					state = expectStatementEnd; // look for rubbish after end
				} else { break; } // not a valid create statement
				continue;
			case expectStatementEnd: // We only get here if we found a token
                     // after the end of the statement, which is an error
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

QString SqlParser::toString(Token t) {
    m_lastToString = nullToken;
    return tos(t);
}

QString SqlParser::brackets(Expression * expr) {
    static const Token open = {"(", tokenOpen};
    static const Token close = {")", tokenClose};
    return tos(open) + tos(expr) + tos(close);
}

QString SqlParser::tos(Expression * expr)
{
	if (expr == 0) { return "<null expression>"; }
	QString s; // used to force evaluation order
	switch (expr->type)
	{
        case exprNull: // empty argument list
            return "";
		case exprToken: // a single operand
			return tos(expr->terminal);
		case exprOpX: // unary-prefix-operator expression
            s = tos(expr->terminal);
			return s + tos(expr->right);
		case exprXOpX: // expression binary-operator expression
			s = tos(expr->left);
            s += tos(expr->terminal);
            return s + tos(expr->right);
		case exprXOp: // expression unary-postfix-operator
			s = tos(expr->left);
            return s + tos(expr->terminal);
		case exprExpr: // ( expression )
			return brackets(expr->left);
		case exprCall: // fname (expression)
			s = tos(expr->terminal);
            return s + brackets(expr->right);
	}
    __builtin_unreachable();
}

QString SqlParser::toString(Expression * expr) {
    m_lastToString = nullToken;
    return tos(expr);
}

// Returns true if *expr can appear inside a DEFAULT expression for a column.
// Note that an interior expression has different rules from a top level one.
bool SqlParser::isValidDefault(Expression * expr) {
    switch (expr->type) {
        case exprNull: return false; // not allowed except as arglist
        case exprToken: switch (expr->terminal.type) {
            case tokenIdentifier:
            case tokenStringLiteral:
            case tokenBlobLiteral:
            case tokenNumeric:
                return true;
            default: return false; // other token types not allowed here
        }
        case exprOpX: return isValidDefault(expr->right);
        case exprXOpX:
            return isValidDefault(expr->left) && isValidDefault(expr->right);
        case exprXOp:
        case exprExpr: return isValidDefault(expr->left);
        case exprCall:
            expr = expr->right;
            return (expr->type == exprNull) || isValidDefault(expr);
    }
    __builtin_unreachable();
}

// Returns true if s is a valid DEFAULT expression for a column
bool SqlParser::isValidDefault(QString input) {
    Expression * expr = parseExpression(input);
    if (expr == 0) { return false; }
    else if (expr->type == exprNull) { return false; }
    // Any token that can be an exprToken is allowed (not operator types)
    else if (expr->type == exprToken) { return true; }
    else if (   (expr->type == exprOpX)
             && (expr->terminal.type == tokenPlusMinus))
    { // + or - followed by literal or identifier is allowed
        expr = expr->right;
        if (expr->type == exprToken) {
            switch (expr->terminal.type) {
                case tokenIdentifier:
                case tokenStringLiteral:
                case tokenBlobLiteral:
                case tokenNumeric:
                    return true;
                default: return false;
            }
        } else { return false; }
    } else if (expr->type == exprExpr) {
        return isValidDefault(expr->left);
    }
    return false; // exprXOpX, exprXOp, exprCall, exprOpX not + or -
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
            QList<FieldInfo>::const_iterator i;
            for (i = m_fields.constBegin(); i != m_fields.constEnd(); ++i) {
				if (first)
				{
					first = false;
					s += Utils::q(i->name);
				}
				else
				{
					s += ", " + Utils::q(i->name);
				}
				if (!i->type.isEmpty())
				{
					s += " " + Utils::q(i->type);
				}
				if (!i->defaultValue.isEmpty())
				{
					if (i->defaultisQuoted)
					{
						s += " DEFAULT " + Utils::q(i->defaultValue, "'");
					}
					else
					{
						s += " DEFAULT " + i->defaultValue;
					}
				}
				if (i->isNotNull) { s += " NOT NULL"; }
				if (i->isAutoIncrement) { s += " PRIMARY KEY AUTOINCREMENT"; }
				else if (i->isPartOfPrimaryKey)
				{
					pk.append(Utils::q(i->name));
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
            QList<Expression *>::const_iterator i;
            for (i = m_columns.constBegin(); i != m_columns.constEnd(); ++i) {
				if (first) { first = false; }
				else { s += ",\n"; }
				s += toString(*i);
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
            case exprNull: return true;
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
						return replaceToken(map, expr);
					default:
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
				return    (expr->right == NULL)
                       || replace(map, expr->right, true);
			case exprExpr: // ( expression )
				return replace(map, expr->left, inside);
		}
	}
	return true;
}

// This is called by AlterTableDialog to update this parser for the
// changes of column name and/or table name caused by doing an alter.
bool SqlParser::replace(QMap<QString,QString> map, QString newTableName)
{
	m_tableName = newTableName;
	bool result = true;
    QList<Expression *>::const_iterator i;
    for (i = m_columns.constBegin(); i != m_columns.constEnd(); ++i) {
		result &= replace(map, *i, false);
	}
	return result & replace(map, m_whereClause, true);
}
