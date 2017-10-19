/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#include "SessionWidget.h"
#include "FileWidget.h"
#include "schema/ExpressToSourceConverter.h"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtCore/QSettings>
#include <QtGui/QKeyEvent>

class NewFileTabWidget : public QWidget
{
public:
	NewFileTabWidget( SessionWidget* mw )
	{
		QPushButton* btn = new QPushButton( "Add EXPRESS schema file" );
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

		connect( btn, SIGNAL( clicked() ), mw, SLOT( slotAddOtherSchemaFileClicked() ) );
	}
	~NewFileTabWidget()
	{

	}
	virtual const char* className() const { return "NewSessionTabWidget"; }

public slots:

private:

};

SessionWidget::SessionWidget(  QString session_name, QWidget *parent ) : QWidget(parent)
{
	m_session_name = session_name;
	m_generator = new ExpressToSourceConverter();
	m_connected_txt_out = false;

	QFile file( ":Resources/styles.css" );
	file.open( QFile::ReadOnly );
	QString styleSheet = QLatin1String( file.readAll() );
	setStyleSheet( styleSheet );

	int left_panel_width = 160;

	m_files_tab = new QTabWidget;
	m_files_tab->setTabsClosable( true );
	connect( m_files_tab, SIGNAL( tabCloseRequested(int) ), this, SLOT( slotTabCloseRequested(int) ) );

	// options
	QLineEdit* le_smart_pointers = new QLineEdit;
	le_smart_pointers->setText( "std::tr1::shared_ptr<T>" ); 
	le_smart_pointers->setDisabled( true );

	m_edit_namespace = new QLineEdit;
	m_edit_namespace->setText( "" );
	m_edit_namespace->setDisabled( true );

	QCheckBox* checkbox_include_comments = new QCheckBox();
	checkbox_include_comments->setChecked( true );
	connect( checkbox_include_comments, SIGNAL( stateChanged(int) ), this, SLOT( slotIncludeComments(int) ) );

	m_checkbox_add_namespace = new QCheckBox();
	connect( m_checkbox_add_namespace, SIGNAL( stateChanged( int ) ), this, SLOT( slotSetNamespace( int ) ) );

	m_checkbox_add_ifcpp_in_path = new QCheckBox();
	connect( m_checkbox_add_ifcpp_in_path, SIGNAL( stateChanged( int ) ), this, SLOT( slotSetIfcPPInPath( int ) ) );
	m_checkbox_add_ifcpp_in_path->setChecked( true );

	// output
	m_le_path_out = new QLineEdit( "D:/work/IfcPlusPlus/IfcPlusPlus/src" );
	connect( m_le_path_out, SIGNAL(textChanged(const QString&)), this, SLOT( slotPathOutChanged(const QString&)));

	m_btn_parse_schema = new QPushButton( "Generate all" );
	connect( m_btn_parse_schema, SIGNAL( clicked() ), this, SLOT( slotParseSchemaClicked() ) );

	m_txt_out = new QTextEdit();
	m_txt_out_error = new QTextEdit();


	m_progress_bar = new QProgressBar();
	m_progress_bar->setRange( 0, 1000 );
	
	QGridLayout* grid_input = new QGridLayout;
	grid_input->addWidget(new QLabel( "<b>Input</b>" ), 0, 0 );
	grid_input->addWidget( m_files_tab, 1, 0 );

	QGridLayout* grid_options = new QGridLayout;
	grid_options->addWidget( new QLabel( "<b>Options</b>" ), 0, 0 );

	grid_options->addWidget( new QLabel( "Smart pointers" ), 0, 1 );
	grid_options->addWidget( le_smart_pointers, 0, 2  );

	grid_options->addWidget( new QLabel( "Add comments" ), 1, 1 );
	grid_options->addWidget( checkbox_include_comments, 1, 2  );

	grid_options->addWidget( new QLabel( "Add namespace" ), 2, 1 );
	grid_options->addWidget( m_checkbox_add_namespace, 2, 2 );
	grid_options->addWidget( m_edit_namespace, 2, 3 );

	grid_options->addWidget( new QLabel( "Add \"ifcpp\" in source path" ), 3, 1 );
	grid_options->addWidget( m_checkbox_add_ifcpp_in_path, 3, 2 );
	
	grid_options->setColumnMinimumWidth( 0, left_panel_width );
	grid_options->setColumnStretch( 0, 0 );
	grid_options->setColumnStretch( 1, 0 );
	grid_options->setColumnStretch( 3, 1 );


	QGridLayout* grid_output = new QGridLayout();
	int row = 0;
	grid_output->addWidget( new QLabel( "<b>Output</b>" ), row, 0 );

	grid_output->addWidget( new QLabel( "Target folder (\"IfcPlusPlus/src/ifcpp/\" in your IfcPlusPlus download)" ), row, 1 );
	grid_output->addWidget( m_le_path_out, row, 2  );

	// subfolders
	++row;
	m_le_subfolder_classes = new QLineEdit();
	grid_output->addWidget( new QLabel( "Subfolder for generated files (cpp,h), default: IFC4" ), row, 1 );

	m_label_path_subfolder_classes = new QLabel();
	QHBoxLayout* subfolder_classes_hbox = new QHBoxLayout();
	subfolder_classes_hbox->addWidget( m_label_path_subfolder_classes );
	subfolder_classes_hbox->addWidget( m_le_subfolder_classes );


	grid_output->addLayout( subfolder_classes_hbox, row, 2  );

	// filename prefix
	m_le_filename_prefix = new QLineEdit( "IfcPP" );
	++row;
	grid_output->addWidget( new QLabel("File name prefix for non-schema files, default: IfcPP"), row, 1 );
	grid_output->addWidget( m_le_filename_prefix, row, 2 );


	++row;
	grid_output->addWidget( m_btn_parse_schema, row, 0  );

	++row;
	grid_output->addWidget( m_txt_out_error,	row, 1, 1, 2 );
	grid_output->setRowStretch( row, 1 );

	++row;
	grid_output->addWidget( m_txt_out,	row, 1, 1, 2 );
	grid_output->setRowStretch( row, 3 );

	++row;
	grid_output->addWidget( m_progress_bar,	row, 1, 1, 2 );
	grid_output->setColumnMinimumWidth( 0, left_panel_width );

	QWidget* input_widget = new QWidget;
	input_widget->setLayout( grid_input );

	QWidget* options_widget = new QWidget;
	options_widget->setLayout( grid_options );

	QWidget* output_widget = new QWidget;
	output_widget->setLayout( grid_output );

	QWidget* vbox_out_widget = new QWidget();
	QVBoxLayout* vbox_out = new QVBoxLayout(vbox_out_widget);
	vbox_out->addWidget( options_widget );
	vbox_out->addWidget( output_widget );

	QSplitter* splitter = new QSplitter( Qt::Vertical );
	splitter->addWidget( input_widget );
	splitter->addWidget( vbox_out_widget );
	splitter->setStretchFactor( 0, 2 );
	splitter->setStretchFactor( 1, 0 );

	QVBoxLayout* main_vbox = new QVBoxLayout();
	main_vbox->setContentsMargins( 0, 0, 0, 0 );
	main_vbox->addWidget( splitter );
	setLayout( main_vbox );

	setWidgetRaised( true );

	QString session_name_settings = session_name;
	session_name_settings.replace( QRegExp("\\s"), "" );
	QSettings settings(QSettings::UserScope, "ExpressToSourceGeneratorSession" + session_name_settings );

	bool clear_settings = false;
	if( clear_settings )
	{
		settings.clear();
	}

	QStringList keys = settings.allKeys();
	if( keys.contains( "SessionWidget/geometry" ) )
	{
		restoreGeometry(settings.value( "SessionWidget/geometry").toByteArray());
	}

	if( keys.contains( "openedFiles" ) )
	{
		QStringList opened_files = settings.value( "openedFiles").toStringList();
		for( int i=0; i<opened_files.size(); ++i )
		{
			QString path_in = opened_files.at( i );
			slotAddSchemaFile( path_in );
		}
	}
	QString label( "+" );
	m_files_tab->insertTab( m_files_tab->count() , new NewFileTabWidget( this ), label );

	if( keys.contains( "pathOut" ) )
	{
		m_le_path_out->setText( settings.value( "pathOut").toString() );
	}
	slotPathOutChanged( m_le_path_out->text() );

	QString subfolder_all("ifcpp");
	if( keys.contains( "subfolderAll" ) )
	{
		subfolder_all = settings.value( "subfolderAll").toString();
	}

	QString classes_subfolder;
	if( keys.contains( "classesOut" ) )
	{
		classes_subfolder = settings.value( "classesOut").toString();
	}

	if( classes_subfolder.length() < 1 )
	{
		classes_subfolder = "IFC4";
	}
	m_le_subfolder_classes->setText( classes_subfolder );

	if( keys.contains( "filenamePrefix" ) )
	{
		m_le_filename_prefix->setText( settings.value( "filenamePrefix").toString() );
	}

	if( keys.contains( "setNamespace" ) )
	{
		QString namesp = settings.value( "setNamespace" ).toString();
		m_edit_namespace->setText( namesp );
	}

	if( keys.contains( "setNamespaceEnabled" ) )
	{
		m_checkbox_add_namespace->setChecked( settings.value( "setNamespaceEnabled" ).toBool() );
	}

	if( keys.contains( "addIfcPPInPath" ) )
	{
		m_checkbox_add_ifcpp_in_path->setChecked( settings.value( "addIfcPPInPath" ).toBool() );
	}
}

SessionWidget::~SessionWidget(){}

void SessionWidget::closeEvent(QCloseEvent *event)
{
	QStringList opened_files;
	for( int i=0; i<m_files_tab->count(); ++i )
	{
		FileWidget* fw = dynamic_cast<FileWidget*>(m_files_tab->widget( i ));
		if( !fw )
		{
			continue;
		}
		opened_files << fw->getPathFile();
	}

	QString session_name_settings = m_session_name;
	session_name_settings.replace( QRegExp("\\s"), "" );
	QSettings settings(QSettings::UserScope, "ExpressToSourceGeneratorSession" + session_name_settings );
	settings.setValue( "SessionWidget/geometry", saveGeometry());
	settings.setValue( "openedFiles",opened_files );
	settings.setValue( "pathOut",m_le_path_out->text() );
	settings.setValue( "classesOut",m_le_subfolder_classes->text() );
	settings.setValue( "filenamePrefix",m_le_filename_prefix->text() );
	settings.setValue( "setNamespaceEnabled", m_checkbox_add_namespace->isChecked() );
	settings.setValue( "setNamespace", m_edit_namespace->text() );
	settings.setValue( "addIfcPPInPath", m_checkbox_add_ifcpp_in_path->isChecked() );
	

	QWidget::closeEvent(event);
}

void SessionWidget::setWidgetRaised( bool raised )
{
	if( raised )
	{
		if( !m_connected_txt_out )
		{
			connect( m_generator, SIGNAL(  signalProgressValue( int ) ),	this, SLOT( slotProgressValue( int ) ) );
			connect( m_generator, SIGNAL( signalTxtOut( QString ) ),		this, SLOT( slotTxtOut( QString ) ) );
			connect( m_generator, SIGNAL( signalTxtOutWarning( QString ) ), this, SLOT( slotTxtOutWarning( QString ) ) );
			m_connected_txt_out = true;
		}
	}
	else
	{
		if( m_connected_txt_out )
		{
			disconnect( m_generator, SIGNAL(  signalProgressValue( int ) ),	this, SLOT( slotProgressValue( int ) ) );
			disconnect( m_generator, SIGNAL( signalTxtOut( QString ) ),		this, SLOT( slotTxtOut( QString ) ) );
			disconnect( m_generator, SIGNAL( signalTxtOutWarning( QString ) ), this, SLOT( slotTxtOutWarning( QString ) ) );
			m_connected_txt_out = false;
		}
	}
}

void SessionWidget::slotAddOtherSchemaFileClicked()
{
	QFileDialog dialog( this );
	dialog.setFileMode( QFileDialog::ExistingFile );

	QStringList fileNames;
	if( dialog.exec() )
	{
		fileNames = dialog.selectedFiles();
	}

	if( fileNames.size() > 0 )
	{
		QString path_in = fileNames[0];
		slotAddSchemaFile( path_in );
		//if( !m_recent_files.contains( path_in ) )
		//{
		//	m_recent_files.push_back( path_in );
		//}
	}
}

int SessionWidget::slotAddSchemaFile( QString path_in )
{
	// sort out duplicates
	for( int i=0; i<m_files_tab->count(); ++i )
	{
		FileWidget* fw = dynamic_cast<FileWidget*>(m_files_tab->widget( i ));
		if( !fw )
		{
			continue;
		}
		QString fw_path = fw->getPathFile();

		if( fw_path == path_in )
		{
			return -1;
		}
	}

	this->setDisabled( true );
	this->repaint();

	QStringList path_in_folders = path_in.split( "/" );
	QString label = path_in;
	if( path_in_folders.size() > 0 )
	{
		label = path_in_folders.at(path_in_folders.size()-1);
	}
	FileWidget* fw = new FileWidget();
	int index = m_files_tab->addTab( fw, label );
	connect( fw,	SIGNAL( signalFileWidgetLoaded() ), this,	SLOT( slotFileWidgetLoaded() ) );
	connect( this,	SIGNAL( signalSaveAllChanges() ), fw,		SLOT( slotSaveClicked() ) );
	fw->setPathFile( path_in );

	return index;

}
void SessionWidget::slotFileWidgetLoaded()
{
	this->setDisabled( false );
}

void SessionWidget::slotTabCloseRequested( int index )
{
	QWidget* w = m_files_tab->widget( index );
	NewFileTabWidget* new_tab_w = dynamic_cast<NewFileTabWidget*>(w);
	if( new_tab_w )
	{
		return;
	}

	m_files_tab->removeTab( index );
}

void SessionWidget::slotParseSchemaClicked()
{
	emit( signalSaveAllChanges() );
	m_txt_out->clear();
	m_txt_out_error->clear();

	// collect files content
	QStringList schema_files_content;
	for( int i=0; i<m_files_tab->count(); ++i )
	{
		FileWidget* fw = dynamic_cast<FileWidget*>( m_files_tab->widget( i ) );
		if( !fw )
		{
			continue;
		}
		QTextEdit* te = fw->getTextEdit();
		schema_files_content << te->toPlainText();
		// TODO: get file name, pass it to class generator
	}

	QString path_out = m_le_path_out->text();
	QString subfolder_classes = m_le_subfolder_classes->text();
	if( subfolder_classes.length() < 1 )
	{
		QMessageBox mbox;
		mbox.setText( "Subfolder for classes needs to be specified" );
		mbox.exec();
		return;
	}
	QString filename_prefix = m_le_filename_prefix->text();

	m_generator->setPathOut( path_out );
	m_generator->setSubfolderClasses( subfolder_classes );
	m_generator->setFilenamePrefix( filename_prefix );

	m_txt_out->clear();
	m_txt_out_error->clear();
	m_generator->parseSchemaFiles( schema_files_content );
}

void SessionWidget::slotTxtOut( QString txt )
{
	m_txt_out->append( txt + "<br/>" );// "<div style=\"color:black;\">" + txt + "</div><br/>" );
	m_txt_out->repaint();
}

void SessionWidget::slotTxtOutWarning( QString txt )
{
	m_txt_out_error->append( "<div style=\"color:#cc3333;\">Warning: " + txt + "</div><br/>" );
}

void SessionWidget::keyPressEvent( QKeyEvent* event )
{
	if( event->key() == Qt::Key_F5 )
	{
		slotParseSchemaClicked();
	}
	else if( event->key() == Qt::Key_F6 )
	{
		close();
	}
}

void SessionWidget::slotProgressValue( int value )
{
	m_progress_bar->setValue( value );
}

void SessionWidget::slotIncludeComments( int state )
{
	if( state == Qt::Checked )
	{
		m_generator->m_include_comments = true;
	}
	else
	{
		m_generator->m_include_comments = false;
	}
}

void SessionWidget::slotSetNamespace( int state )
{
	if( state == Qt::Checked )
	{
		m_generator->m_schema_namespace = m_edit_namespace->text();
		m_edit_namespace->setEnabled( true );
	}
	else
	{
		m_generator->m_schema_namespace = "";
		m_edit_namespace->setEnabled( false );
	}
}

void SessionWidget::slotSetIfcPPInPath( int state )
{
	if( state == Qt::Checked )
	{
		m_generator->m_add_ifcpp_in_path = true;
	}
	else
	{
		m_generator->m_add_ifcpp_in_path = false;
	}
}

void SessionWidget::slotPathOutChanged( const QString& txt )
{
	QString path_out = txt;
	if( !path_out.endsWith( "\\" ) )
	{
		path_out = path_out + "\\";
	}
	
	// update also path to classes out folder
	QString path_out_classes = path_out;
	if( !path_out_classes.endsWith( "\\" ) )
	{
		path_out_classes = path_out_classes + "\\";
	}
	m_label_path_subfolder_classes->setText( path_out_classes );
}
