/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#pragma once

#include <QtCore/qglobal.h>
#include <QWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QLineEdit>
#include <QLabel>
#include <QTextedit>
#include <QProgressBar>
#include <QCheckBox>

class ExpressToSourceConverter;

class SessionWidget : public QWidget
{
	Q_OBJECT

public:
	SessionWidget( QString session_name, QWidget *parent = 0 );
	~SessionWidget();

	virtual const char* classname() const { return "SessionWidget"; }
	void closeEvent( QCloseEvent* event );
	void setWidgetRaised( bool raised );

signals:
	void signalSaveAllChanges();

public slots:
	void slotTxtOut( QString txt );
	void slotTxtOutWarning( QString txt );
	void slotProgressValue( int );
	int slotAddSchemaFile( QString path );
	void slotFileWidgetLoaded();
	void slotIncludeComments(int);
	void slotSetNamespace( int );
	void slotSetIfcPPInPath( int );
	void slotPathOutChanged( const QString& txt );

protected:
	void keyPressEvent( QKeyEvent* event );

private:
	ExpressToSourceConverter*	m_generator;
	QString				m_session_name;
	QPushButton*		m_btn_parse_schema;
	QTabWidget*			m_files_tab;
	QLineEdit*			m_le_path_out;
	QLineEdit*			m_le_subfolder_classes;
	QLineEdit*			m_edit_namespace;
	QLabel*				m_label_path_subfolder_classes;
	QLineEdit*			m_le_filename_prefix;
	QCheckBox*			m_checkbox_add_namespace;
	QCheckBox*			m_checkbox_add_ifcpp_in_path;

	QTextEdit*			m_txt_out;
	QTextEdit*			m_txt_out_error;
	QProgressBar*		m_progress_bar;
	bool				m_connected_txt_out;
	std::map<QPushButton*, int> m_map_btnload_tablerow;

private slots:
	void slotAddOtherSchemaFileClicked();
	void slotParseSchemaClicked();
	void slotTabCloseRequested( int index );
};
