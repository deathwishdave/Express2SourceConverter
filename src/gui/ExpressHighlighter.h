/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */
#pragma once

#include <QtGui/QSyntaxHighlighter>

class ExpressHighlighter : public QSyntaxHighlighter
 {
     Q_OBJECT

 public:
     ExpressHighlighter(QTextDocument *parent = 0);

 protected:
     void highlightBlock(const QString &text);

 private:
     struct HighlightingRule
     {
         QRegExp pattern;
         QTextCharFormat format;
     };
     QVector<HighlightingRule> m_highlightingRules;

     QRegExp m_commentStartExpression;
     QRegExp m_commentEndExpression;

     QRegExp m_functionStartExpression;
     QRegExp m_functionEndExpression;

	 QTextCharFormat m_keywordFormat;
     QTextCharFormat m_classFormat;
     QTextCharFormat m_singleLineCommentFormat;
     QTextCharFormat m_multiLineCommentFormat;
     QTextCharFormat m_quotationFormat;
     QTextCharFormat m_functionFormat;
};
