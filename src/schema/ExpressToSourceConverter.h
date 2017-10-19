/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#pragma once

#include <map>
#include <QtCore/QObject>
#include "shared_ptr.h"

class QStringList;
class QString;

class Type;
class Entity;
class SelectType;

class IfcPPBasicType
{
public:
	IfcPPBasicType( QString EXPRESS_typename, QString CPP_typename, QString IFCPP_typename )
	{
		m_EXPRESS_typename = EXPRESS_typename;
		m_CPP_typename = CPP_typename;
		m_IFCPP_typename = IFCPP_typename;

	}
	QString m_EXPRESS_typename;
	QString m_CPP_typename;
	QString m_IFCPP_typename;
};

class ExpressToSourceConverter : public QObject
{
	Q_OBJECT

public:
	static const bool SELECT_DERIVE = true;

	ExpressToSourceConverter();
	~ExpressToSourceConverter();

	void parseSchemaFiles( QStringList content_in );
	void parseSchema( QString content );
	void processTypesAndEntities();
	void setPathOut( QString path_out );
	void setSubfolderClasses( QString subfolder );
	QString	getSubfolderIfcClasses() { return m_subfolder_classes; }
	void setFilenamePrefix( QString pre );
	const QString& getFilenamePrefix() { return m_filename_prefix; }

	bool findConstants( QString& file_content, int complete_size, QString schema_version );
	bool findTypes( QString& file_content, int complete_size, QString schema_version );
	bool findEntities( QString& file_content, int complete_size, QString schema_version );
	bool findFunctions( QString& file_content, int complete_size, QString schema_version );
		
	bool writeEntityClassFiles();
	void writeTypeFactory();
	void writeEntityFactory();
	void writeCMakeFile();
	void writeFile( QString path, QString& content );
	
	std::map<QString, shared_ptr<IfcPPBasicType> >	m_basic_types;
	std::map<QString,shared_ptr<Type> >				m_map_types;
	std::map<QString,shared_ptr<Entity> >			m_map_entities;
	QString											m_schema_namespace;
	bool											m_include_comments = true;
	bool											m_code_reset_attributes = false;
	bool											m_add_ifcpp_in_path = true;
	bool											m_always_put_map_in_type_reader = true;
	QString											m_header_comment;
	QString											m_path_out;
	QString											m_subfolder_classes;
	QString											m_filename_prefix;
	int												m_num_skipped_files = 0;

signals:
	void signalTxtOut( QString out );
	void signalTxtOutWarning( QString out );
	void signalProgressValue( int );

public slots:
	void slotEmitTxtOut( QString txt );
	void slotEmitTxtOutWarning( QString txt );

};
