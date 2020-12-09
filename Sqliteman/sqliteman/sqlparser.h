/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2015 Richard P. Parkins, M. A., and released under the GPL.

This class parses SQL schemas (not general SQL statements). Since the
schema is known to be valid, we do not always detect bad syntax.
*/

#ifndef SQLPARSER_H
#define SQLPARSER_H

enum tokenType {
	tokenNone,
	tokenIdentifier,
	tokenQuotedIdentifier,
	tokenSquareIdentifier,
	tokenBackQuotedIdentifier,
	tokenStringLiteral,
	tokenBlobLiteral,
	tokenNumeric,
	tokenOperator,
	tokenPostfix,
	tokenSingle
};

enum itemType {
	createTable,
	createIndex
};

// To simplify our parsing, we treat commas and some keywords as operators.
enum exprType {
	exprToken, // a single operand
	exprOpX, // unary-prefix-operator expression
	exprXOpX, // expression binary-operator expression
	exprXOp, // expression unary-postfix-operator
	exprCall, // fname (expression)
	exprExpr // ( expression )
};

typedef struct
{
	QString name;
	enum tokenType type;
	bool isColName;
} Token;

typedef struct Expression {
	enum exprType type;
	struct Expression * left;
	Token terminal;
	struct Expression * right;
} Expression;

typedef struct
{
	QString name;
	QString type;
    QString referencedTable;
    QStringList referencedKeys;
	QString defaultValue;
	bool defaultIsExpression;
	bool defaultisQuoted;
	bool isPartOfPrimaryKey;
	bool isWholePrimaryKey;
	bool isColumnPkDesc;
	bool isTablePkDesc;
	bool isAutoIncrement;
	bool isNotNull;
} FieldInfo;

class SqlParser
{
	public:
		bool m_isValid; // false -> couldn't parse CREATE statement
		enum itemType m_type;
		bool m_isUnique; // UNIQUE (index only)
		bool m_hasRowid; // true -> not WITHOUT ROWID (or not a table)
		QString m_indexName;
		QString m_tableName;
		QList<FieldInfo> m_fields;  // table only
		QList<Expression *> m_columns; // index only
		Expression * m_whereClause; // index only

	private:
		QStringList operators;
		QStringList posts;
		int m_depth;
		QList<Token> tokenise(QString input);
		void clearField(FieldInfo &f);
		void addToPrimaryKey(QString s);
		void addToPrimaryKey(FieldInfo &f);
		void destroyExpression(Expression * e);
		Expression * parseExpression(QList<Token> & tokens, QStringList ends);
		// replacing column names, returns false if any deleted
		bool replaceToken(QMap<QString,QString> map, Expression * expr);
		bool replace(QMap<QString,QString> map,
					 Expression * expr, bool inside = false);

	public:
		SqlParser(QString input);
		~SqlParser();
		static QString defaultToken(FieldInfo &f);
		bool replace(QMap<QString,QString> map, QString newTableName);
		static QString toString(Token t);
		static QString toString(Expression * expr);
		QString toString();
};

#endif // SQLPARSER_H
