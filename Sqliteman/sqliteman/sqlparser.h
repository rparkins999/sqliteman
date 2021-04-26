/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2021 Richard P. Parkins, M. A., and released under the GPL.

This class parses SQL schemas (not general SQL statements). Since the
schema is known to be valid, we do not always detect bad syntax.
*/

// This bit of fantasy enables my pd prettyprinter to print enum values
#ifdef ENUMPRINT
#undef SQLPARSER_H
#undef ENUM
#undef ENUMVALUE
#undef ENUMLAST
#define ENUM(type)  static QString prepare##type(enum type x) { switch (x) {
#define ENUMVALUE(x) case x: return #x;
#define ENUMLAST(x) case x: return #x; default: return NULL; } }
#endif

#ifndef SQLPARSER_H

#ifndef ENUMPRINT
#undef ENUM
#undef ENUMVALUE
#undef ENUMLAST
#define ENUM(type) enum type {
#define ENUMVALUE(x) x,
#define ENUMLAST(x) x };
#endif

ENUM(tokenType)
ENUMVALUE(tokenQuotedIdentifier) // "identifier"
ENUMVALUE(tokenSquareIdentifier) // [identifier]
ENUMVALUE(tokenBackQuotedIdentifier) //`identifier`
ENUMVALUE(tokenStringLiteral) // 'anything'
ENUMVALUE(tokenBlobLiteral)
ENUMVALUE(tokenIdentifier)
ENUMVALUE(tokenOperatorA) // binary operator (alphanumeric)
ENUMVALUE(tokenPostfixA) // (see QStringList posts)
ENUMVALUE(tokenPrefixA) // NOT, NOT EXISTS
ENUMVALUE(tokenNumeric)
ENUMVALUE(tokenOpen) // (
ENUMVALUE(tokenComma) // ,
ENUMVALUE(tokenClose) // )
ENUMVALUE(tokenOperator) // binary operator
ENUMVALUE(tokenPrefix) // ~
ENUMVALUE(tokenPlusMinus) // can be prefix or binary opeator
ENUMVALUE(tokenNOT) // NOT, can be prefix or part of a longer operator
ENUMVALUE(tokenNone) // never returned by tokenise()
ENUMLAST(tokenInvalid) // unterminated quote, bad number, invalid character

// To simplify our parsing, we treat commas and some keywords as operators.
ENUM(exprType)
ENUMVALUE(exprNull) // empty expression
ENUMVALUE(exprToken) // a single operand
ENUMVALUE(exprOpX) // unary-prefix-operator expression
ENUMVALUE(exprXOpX) // expression binary-operator expression
ENUMVALUE(exprXOp) // expression unary-postfix-operator
ENUMVALUE(exprExpr) // ( expression )
ENUMLAST(exprCall) // fname (expression)

#ifndef ENUMPRINT

enum itemType {
	createTable,
	createIndex
};

typedef struct Token {
	QString name;
	enum tokenType type;
} Token;

typedef struct Expression {
	enum exprType type;
	struct Expression * left;
	Token terminal;
	struct Expression * right;
} Expression;

typedef struct FieldInfo {
	QString name;
	QString type;
    QString collator;
    QString referencedTable;
    QStringList referencedKeys;
	QString defaultValue;
    // 64-bit signed integer, set and updated in sqlmodels.cpp
    qlonglong defaultKeyValue;
	bool defaultIsExpression;
	bool defaultisQuoted;
	bool isPartOfPrimaryKey;
	bool isWholePrimaryKey;
	bool isColumnPkDesc;
	bool isTablePkDesc;
	bool isAutoIncrement;
	bool isNotNull;
    bool isUnique;
} FieldInfo;

class SqlParser
{
	public:
		bool m_isUnique; // UNIQUE (index only)
		bool m_hasRowid; // true -> not WITHOUT ROWID (or not a table)
		QString m_indexName;
		QString m_tableName;
		QList<FieldInfo> m_fields;  // table only
		QList<Expression *> m_columns; // index only
		Expression * m_whereClause; // index only

	private:
		int m_depth;
        enum itemType m_type;
        QList<Token> m_tokens;
		QList<Token> tokenise(QString input);
		void destroyExpression(Expression * e);
        Expression * internalParser(int state, int level, QStringList ends);
        Expression * parseExpression(QString input);
        void clearField(FieldInfo &f);
		void addToPrimaryKey(QString s);
		void addToPrimaryKey(FieldInfo &f);
		static QString tos(Token t);
        static QString brackets(Expression * expr);
		static QString tos(Expression * expr);
        bool isValidDefault(Expression * expr);
        // replacing column names, returns false if any deleted
		bool replaceToken(QMap<QString,QString> map, Expression * expr);
		bool replace(QMap<QString,QString> map,
					 Expression * expr, bool inside = false);

	public:
        SqlParser();
        SqlParser(QString input);
        ~SqlParser();
		static QString defaultToken(FieldInfo &f);
		static QString toString(Token t);
		static QString toString(Expression * expr);
        bool isValidDefault(QString input);
		QString toString();
 		bool replace(QMap<QString,QString> map, QString newTableName);
};

#endif // ENUMPRINT
#endif // SQLPARSER_H
#define SQLPARSER_H
