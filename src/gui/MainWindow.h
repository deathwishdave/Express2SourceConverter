/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#pragma once

#include <QtCore/qglobal.h>
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets/qtabwidget.h>
#include <QtCore/qstringlist.h>

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow( QWidget *parent = 0 );
	~MainWindow();

public slots:
	void slotNewSessionClicked();

protected:
	void closeEvent( QCloseEvent* event );

private:
	QTabWidget*			m_sessions_tab;
	QStringList			m_session_names;

private slots:
	void slotCurrentTabChanged( int index );
	void slotTabCloseRequested( int index );
};
