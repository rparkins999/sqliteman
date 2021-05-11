/*
For general Sqliteman copyright and licensing information please refer
to the COPYING file provided with the program. Following this notice may exist
a copyright and/or license notice that predates the release of Sqliteman
for which a new license (GPL+exception) is in place.

This file written by and copyright © 2021 Richard P. Parkins, M. A., and released under the GPL.

This class parses SQL schemas (not general SQL statements). Since the
schema is known to be valid, we do not always detect bad syntax.
*/

#ifndef SQLPARSER_H

// This make a normal enum definition when this file isn't included from pd.h
#ifndef ENUM
#define ENUM(t) enum t {
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

// Main SQL statement parser
ENUM(sqlParserState)
ENUMVALUE(expectCREATE) // at present we only handle CREATE TABLE or INDEX
// TEMP or TEMPORARY isn't preserved in the schema
ENUMVALUE(expectCreated) // TABLE or UNIQUE or INDEX
// we don't handle views or triggers yet
// IF NOT EXISTS and database name don't get copied to the schema
// from here we are creating a table
ENUMVALUE(TexpectTableName) // name of the table
// CREATE TABLE AS SELECT ... is translated into CREATE TABLE (...)
ENUMVALUE(TexpectColumnList) // expect '('
ENUMVALUE(TexpectColumnName) // name of the column
ENUMVALUE(TexpectColOrTC) // seen ',' expect another column or table constraint
ENUMVALUE(TexpectColumnType) // expect type or column constraint or ',' or ')'
ENUMVALUE(TexpectTypeQualifier) // ... or column constraint or , or )
ENUMVALUE(TexpectFieldWidth1) // after '(' expect sign or number
ENUMVALUE(TexpectFieldWidth1N) // after sign, must have number
ENUMVALUE(TexpectFieldWidthSep) // expect ',' or ')'
ENUMVALUE(TexpectFieldWidth2) // after ',' expect sign or number
ENUMVALUE(TexpectFieldWidth2N) // after sign, must have number
ENUMVALUE(TexpectFieldWidthClose) // after "nnn,nnn", must have ')'
ENUMVALUE(TexpectColConstraint) // after ("nnn,nnn") expect constraint
// from here we are decoding a column constraint in a table
ENUMVALUE(CCexpectName) // after CONSTRAINT expect name
ENUMVALUE(CCexpectType) // after CONSTRAINT name expect constraint type
ENUMVALUE(CCexpectKEY) // after PRIMARY expect KEY
ENUMVALUE(CCexpectPKqualifier) // after PRIMARY KEY
ENUMVALUE(CCexpectPKConflict) // after PRIMARY KEY expect conflict clause or next
ENUMVALUE(CCexpectPKCONFLICT) // after ON expect CONFLICT
ENUMVALUE(CCexpectPKConflictAction) // after ON CONFLICT expect action
ENUMVALUE(CCexpectAUTO) // after CONFLICT action expect AUTOINCREMENT or next
ENUMVALUE(CCexpectNULL) // after NOT expect NULL
ENUMVALUE(CCexpectConflict) // expect conflict clause or next
ENUMVALUE(CCexpectCONFLICT) // after ON expect CONFLICT
ENUMVALUE(CCexpectConflictAction) // after ON CONFLICT expect action
ENUMVALUE(CCexpectOpenExprClose) // after CHECK expect (expression)
ENUMVALUE(CCexpectExprClose) // after '(' expect expression ')'
ENUMVALUE(CCexpectDefault) // after DEFAULT expect default value
ENUMVALUE(CCexpectDefaultExprClose) // after DEFAULT '(' expect expression ')'
ENUMVALUE(CCexpectDefaultLiteral) // after DEFAULT '+' or '-' expect number
ENUMVALUE(CCexpectCollateName) // after COLLATE expect name
ENUMVALUE(CCexpectREFTable) // after REFERENCES expect table name
// from here we are decoding a foreign key column constraint
// a column constraint can only reference one foreign key column
ENUMVALUE(CCexpectREFOpen) // expect '(' or foreign key action or next
ENUMVALUE(CCexpectREFColumn) // after '(' expect column name
ENUMVALUE(CCexpectREFClose) // after column name expect ')'
ENUMVALUE(CCexpectFKType) // expect ON, MATCH, (NOT) DEFERRABLE, or next
ENUMVALUE(CCexpectFKDeleteUpdate) // after ON expect DELETE or UPDATE
ENUMVALUE(CCexpectFKDUAction) // after DELETE or UPDATE expect action
ENUMVALUE(CCexpectFKSetAction) // after SET expect NULL or DEFAULT
ENUMVALUE(CCexpectFKACTION) // after NO expect ACTION
ENUMVALUE(CCexpectFKMatchType) // after MATCH expect SIMPLE or PARTIAL or FULL
// NOT NULL can be a column constraint
ENUMVALUE(CCexpectFKDEFERRABLE) // after NOT expect DEFERRABLE or NULL
ENUMVALUE(CCexpectFKINITIALLY) // after DEFERRABLE expect INITIALLY or next
ENUMVALUE(CCexpectFKDeferType) // after INITIALLY expect DEFERRED or IMMEDIATE
// from here we are decoding a table constraint
ENUMVALUE(TCexpectName) // after ',' CONSTRAINT expect table constraint name
ENUMVALUE(TCexpectType) // after table constraint name expect constraint type
ENUMVALUE(TCexpectPKEY) // after PRIMARY expect KEY
ENUMVALUE(TCexpectPKOpen) // after PRIMARY KEY expect '('
ENUMVALUE(TCexpectPKColumn) // expect next column in primary key list
ENUMVALUE(TCexpectPKQualifier) // expect COLLATE or ASC or DESC or next
ENUMVALUE(TCexpectPKCollateName) // after COLLATE expect name
ENUMVALUE(TCexpectPKAscDesc) // expect ASC or DESC or next
ENUMVALUE(TCexpectPKNext) // expect ',' or ')'
ENUMVALUE(TCexpectUOpen) // after UNIQUE expect '('
ENUMVALUE(TCexpectUColumn) // expect next column in primary key list
ENUMVALUE(TCexpectUQualifier) // expect COLLATE or ASC or DESC or next
ENUMVALUE(TCexpectUCollateName) // after COLLATE expect name
ENUMVALUE(TCexpectUAscDesc) // expect ASC or DESC or next
ENUMVALUE(TCexpectUNext) // expect ',' or ')'
ENUMVALUE(TCexpectONorNext) // expect ON or comma or ')'
ENUMVALUE(TCexpectCONFLICT) // after ON expect CONFLICT
ENUMVALUE(TCexpectConflictAction) // after ON CONFLICT epect action
ENUMVALUE(TCexpectCheckOpen) // expect '(' after CHECK
ENUMVALUE(TCexpectCheckExpr) // expect expression after CHECK (
// from here we are decoding a foreign key table constraint
ENUMVALUE(TCexpectFKEY) // after FOREIGN expect KEY
ENUMVALUE(TCexpectFKOpen) // after FOREIGN KEY expect '('
ENUMVALUE(TCexpectFKColumn) // in this-table column list, expect column name
ENUMVALUE(TCexpectFKClose) // in this-table column list, expect ',' or ')'
ENUMVALUE(TCexpectREFERENCES) // expect REFERENCES
ENUMVALUE(TCexpectREFTable) // after REFERENCES expect foreign table name
ENUMVALUE(TCexpectREFOpen) // after table name expect referenced column list
ENUMVALUE(TCexpectREFColumn) // after '(' or ',' expect column name
ENUMVALUE(TCexpectREFClose) // after column name expect ',' or ')'
ENUMVALUE(TCexpectFKType) // expect ON, MATCH, (NOT) DEFERRABLE, or next
ENUMVALUE(TCexpectFKDeleteUpdate) // after ON expect DELETE or UPDATE
ENUMVALUE(TCexpectFKDUAction) // after DELETE or UPDATE expect action
ENUMVALUE(TCexpectFKSetAction) // after SET expect NULL or DEFAULT
ENUMVALUE(TCexpectACTION) // after NO expect ACTION
ENUMVALUE(TCexpectFKMatchType) // after MATCH expect SIMPLE or PARTIAL or FULL
ENUMVALUE(TCexpectFKDEFERRABLE) // after NOT expect DEFERRABLE
ENUMVALUE(TCexpectFKINITIALLY) // after DEFERRABLE expect INITIALLY or next
ENUMVALUE(TCexpectFKDeferType) // after INITIALLY expect DEFERRED or IMMEDIATE
ENUMVALUE(TCexpectAnotherOrEnd) // look for another table constraint or end
ENUMVALUE(TexpectTableConstraint) // after comma expect 
// from here we are creating a table
ENUMVALUE(TexpectWITHOUT) // only allow WITHOUT ROWID or end
ENUMVALUE(TexpectROWID) // after WITHOUT expect ROWID
// from here we are creating an index
ENUMVALUE(IexpectINDEX) // after CREATE UNIQUE
ENUMVALUE(IexpectIndexName) // after CREATE INDEX
ENUMVALUE(IexpectON) // after CREATE INDEX name
ENUMVALUE(IexpectTableName) // after CREATE INDEX on name, expect table name
ENUMVALUE(IexpectColumnList)
ENUMVALUE(IexpectIndexedColumn)
ENUMVALUE(IexpectWHERE)
ENUMVALUE(IexpectWhereExpression)
// here we should ahev finished, so any more tokens are an error
ENUMLAST(expectStatementEnd)

#ifndef ENUMSKIP

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

#endif // ENUMSKIP
#endif // SQLPARSER_H
#define SQLPARSER_H
