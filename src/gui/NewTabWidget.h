/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#pragma once
#include <QtWidgets/QWidget>
class MainWindow;

class NewTabWidget : public QWidget
{
public:
	NewTabWidget( MainWindow* mw )
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
	~NewTabWidget()
	{

	}
	virtual const char* classname() const { return "NewTabWidget"; }

public slots:

private:

};
