/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#include "MainWindow.h"
#include "FileWidget.h"
#include "SessionWidget.h"
#include "schema/ExpressToSourceConverter.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QInputDialog>
#include <QtCore/QFile>
#include <QtCore/QSettings>


class NewSessionTabWidget : public QWidget
{
public:
	NewSessionTabWidget( MainWindow* mw )
	{
		QPushButton* btn = new QPushButton( "Create new session" );
		QVBoxLayout* vbox = new QVBoxLayout();
		vbox->addStretch( 1 );
		vbox->addWidget( btn );
		vbox->addStretch( 1 );

		QHBoxLayout* hbox = new QHBoxLayout();
		hbox->addStretch( 1 );
		hbox->addLayout( vbox, 0 );
		hbox->addStretch( 1 );

		setContentsMargins( 50, 50, 50, 50 );
		setLayout( hbox );

		connect( btn, SIGNAL( clicked() ), mw, SLOT( slotNewSessionClicked() ) );
	}
	~NewSessionTabWidget()
	{

	}
	virtual const char* classname() const { return "NewSessionTabWidget"; }

public slots:

private:

};

MainWindow::MainWindow( QWidget *parent ) : QMainWindow(parent)
{
	setWindowTitle( QString("ExpressToSourceGenerator - Class and Parser Generator - www.ifcquery.com") );
	setWindowIcon( QIcon( ":Resources/img/IfcPlusPlusWindowIcon.png" ) );
	QFile file( ":Resources/styles.css" );
	file.open( QFile::ReadOnly );
	QString styleSheet = QLatin1String( file.readAll() );
	setStyleSheet( styleSheet );

	m_sessions_tab = new QTabWidget();
	m_sessions_tab->setTabsClosable( true );

	QString label( "+" );
	m_sessions_tab->insertTab( 0, new NewSessionTabWidget( this ), label );


	QStatusBar* status = new QStatusBar();
	status->setSizeGripEnabled( true );
	setStatusBar( status );
	setContentsMargins( 0, 0, 0, 0 );
	setCentralWidget( m_sessions_tab );

	QSettings settings(QSettings::UserScope, QLatin1String("ExpressToSourceGenerator"));

	bool clear_settings = false;
	if( clear_settings )
	{
		settings.clear();
	}

	QStringList keys = settings.allKeys();
	if( keys.contains( "MainWindow/geometry" ) )
	{
		restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
	}
	else
	{
		setGeometry( 50, 50, 700, 900 );
	}

	//if( keys.contains( "sessionIds" ) )
	{
		//m_session_ids = settings.value("sessionIds").toStringList();
		if( keys.contains( "sessionNames" ) )
		{
			m_session_names = settings.value("sessionNames").toStringList();
			//int num_sessions = std::min( m_session_ids.size(), m_session_names.size() );
			int num_sessions = m_session_names.size();
			for( int i=0; i<num_sessions; ++i )
			{
				//QString session_id = m_session_ids.at(i);
				QString session_name = m_session_names.at(i);
				SessionWidget* sw = new SessionWidget( session_name, /*m_mw_generator,*/ this );
				m_sessions_tab->insertTab( 0, sw, "---" + session_name + "---" );
				m_sessions_tab->setCurrentIndex( 0 );

			}
		}
	}
	if( keys.contains( "currentSessionTabIndex" ) )
	{
		int saved_current_index = settings.value("currentSessionTabIndex").toInt();
		m_sessions_tab->setCurrentIndex( saved_current_index );
	}

	connect( m_sessions_tab, SIGNAL( tabCloseRequested(int) ), this, SLOT( slotTabCloseRequested(int) ) );
	connect( m_sessions_tab, SIGNAL( currentChanged(int) ),	this, SLOT( slotCurrentTabChanged(int) ) );

}

MainWindow::~MainWindow()
{

}

void MainWindow::closeEvent(QCloseEvent *event)
{
	QSettings settings(QSettings::UserScope, QLatin1String("ExpressToSourceGenerator"));
	settings.setValue("MainWindow/geometry", saveGeometry());
//	settings.setValue( "sessionIds", m_session_ids );
	settings.setValue( "sessionNames", m_session_names );


	for( int i=0; i<m_sessions_tab->count(); ++i )
	{
		QWidget* w = m_sessions_tab->widget( i );
		SessionWidget* session_w = dynamic_cast<SessionWidget*>(w);
		if( session_w )
		{
			session_w->closeEvent( event );
		}
	}

	QWidget::closeEvent(event);
}


void MainWindow::slotTabCloseRequested( int index )
{
	QWidget* w = m_sessions_tab->widget( index );
	NewSessionTabWidget* new_tab_w = dynamic_cast<NewSessionTabWidget*>(w);
	if( new_tab_w )
	{
		return;
	}

	for( int i=0; i<m_sessions_tab->count(); ++i )
	{
		QWidget* w = m_sessions_tab->widget( i );
		SessionWidget* session_w = dynamic_cast<SessionWidget*>(w);
		if( session_w )
		{
			m_sessions_tab->removeTab( i );
			//session_w->closeEvent( event );

			if( i < m_session_names.size() )
			{
				m_session_names.removeAt( i );
			}
		}
	}

	//m_sessions_tab->removeTab( index );
	//if( index < m_session_ids.size() )
	//{
	//	m_session_ids.removeAt( index );
	//}

}

void MainWindow::slotNewSessionClicked()
{
	bool ok;
	QString session_name = QInputDialog::getText(this, tr("Create new session"),tr("Session name:"), QLineEdit::Normal,"IFC4", &ok);
	if( ok )
	{
		int num_sessions = m_session_names.size();
		//while( m_session_ids.size() > num_sessions ) { m_session_ids.pop_back(); }
		while( m_session_names.size() > num_sessions ) { m_session_names.pop_back(); }

		//QString session_id = QString::number( num_sessions + 1 );
		//m_session_ids.push_back( session_id );
		m_session_names.push_back( session_name );

		SessionWidget* sw = new SessionWidget( session_name, /*m_mw_generator,*/ this );
		m_sessions_tab->insertTab( 0, sw, session_name );
		m_sessions_tab->setCurrentIndex( 0 );
	}
}

void MainWindow::slotCurrentTabChanged( int index )
{
	for( int i=0; i<m_sessions_tab->count(); ++i )
	{
		QWidget* w = m_sessions_tab->widget( i );
		SessionWidget* session_w = dynamic_cast<SessionWidget*>(w);
		if( session_w )
		{
			if( i==index )
			{
				session_w->setWidgetRaised( true );
			}
			else
			{
				session_w->setWidgetRaised( false );
			}
		}
	}

	QSettings settings(QSettings::UserScope, QLatin1String("ExpressToSourceGenerator"));
	settings.setValue( "currentSessionTabIndex", index );
}
