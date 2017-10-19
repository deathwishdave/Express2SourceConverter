/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#pragma once

#include <QtWidgets\QWidget>
#include <QtWidgets\QTextEdit>
#include <QtWidgets\QLineEdit>
#include <QtWidgets\QPushButton>

class ExpressToSourceConverter;

class FileWidget : public QWidget
{
	Q_OBJECT

public:
	FileWidget();
	~FileWidget();

	QString	getPathFile() { return m_path_file; }
	void	setPathFile( QString path_file );
	QTextEdit* getTextEdit() { return m_textedit; }

public slots:
	void slotSaveClicked();
	void slotTextChanged();

private:
	QLineEdit*	m_le_path_schema_file;
	QString		m_path_file;
	QTextEdit*	m_textedit;
	QPushButton* m_btn_save;
	bool		m_have_unsaved_changes;

protected:
	void keyPressEvent( QKeyEvent * event );

signals:
	void signalFileWidgetLoaded();
};
