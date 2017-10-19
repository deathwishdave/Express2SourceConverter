/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#include "FileWidget.h"
#include "ExpressHighlighter.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMessageBox>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtGui/QKeyEvent>

FileWidget::FileWidget()
{
	m_have_unsaved_changes = false;
	m_le_path_schema_file = new QLineEdit;
	m_le_path_schema_file->setReadOnly( true );

	QFont font;
	font.setFamily("Courier");
	font.setFixedPitch(true);
	font.setPointSize(10);
	
	m_textedit = new QTextEdit;
	m_textedit->setFont(font);
	m_textedit->setTabStopWidth( 20 );
#ifndef _DEBUG
	ExpressHighlighter* highlighter = new ExpressHighlighter( m_textedit->document() );
#endif
	connect( m_textedit, SIGNAL( textChanged() ), this, SLOT( slotTextChanged() ) );

	m_btn_save = new QPushButton();
	m_btn_save->setIcon(  QIcon( ":Resources/img/save.png" ) );
	m_btn_save->setDisabled( true );
	m_btn_save->setContentsMargins( 2, 2, 2, 2 );
	connect( m_btn_save, SIGNAL( clicked() ), this, SLOT( slotSaveClicked() ) );

	QHBoxLayout* hbox = new QHBoxLayout();
	hbox->addWidget( m_le_path_schema_file, 1 );
	hbox->addWidget( m_btn_save, 0 );

	QVBoxLayout* vbox = new QVBoxLayout();
	vbox->addLayout( hbox ); 
	vbox->addWidget( m_textedit, 0 );
	setLayout( vbox );
}

FileWidget::~FileWidget(){}

void FileWidget::setPathFile( QString path_file )
{
	m_path_file = path_file;
	m_le_path_schema_file->setText( m_path_file );
	m_textedit->blockSignals( true );

	QFile file( path_file );
	if( !file.open(QIODevice::ReadOnly | QIODevice::Text) )
	{
		m_textedit->setText( "couldn't open file " + path_file );
	}
	else
	{
		QTextStream file_stream( &file );
		QString file_content = file_stream.readAll();
		file.close();

		m_textedit->setText( file_content );
		
		if( path_file.startsWith( ":" ) )
		{
			m_textedit->setReadOnly( true );
		}
	}
	emit( signalFileWidgetLoaded() );
	m_textedit->blockSignals( false );
}

void FileWidget::slotTextChanged()
{
	if( !m_path_file.startsWith( ":" ) )
	{
		m_btn_save->setDisabled( false );
	}
	m_have_unsaved_changes = true;
}

void FileWidget::slotSaveClicked()
{
	if( !m_have_unsaved_changes )
	{
		return;
	}

	if( m_path_file.startsWith( ":" ) )
	{
		return;
	}

	QFile file( m_path_file );
	if( !file.open(QFile::WriteOnly|QFile::Truncate|QIODevice::Text) )
	{
		QMessageBox msgBox;
		msgBox.setText( "couldn't open file " + m_path_file );
		msgBox.exec();
		return;
	}

	QString file_content = m_textedit->toPlainText();

	QTextStream file_stream( &file );
	file_stream << file_content;
	file.close();
	m_btn_save->setDisabled( true );
}

void FileWidget::keyPressEvent( QKeyEvent * event )
{
	if( event->modifiers() == Qt::CTRL )
	{
		if( event->key() == Qt::Key_S )
		{
			slotSaveClicked();
		}
	}
}
