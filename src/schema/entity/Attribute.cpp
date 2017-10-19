/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#include "schema/ExpressToSourceConverter.h"
#include "schema/entity/Attribute.h"
#include "schema/type/Type.h"
#include "Entity.h"

// TYPE
QString AttributeType::getDataType()
{
	return m_type->m_className;
}

QString AttributeType::getDataTypeWithSmartPointer(bool weak)
{
	if(weak)
	{
		return "weak_ptr<" + m_type->m_className + ">";
	}
	else
	{
		return "shared_ptr<" + m_type->m_className + ">";
	}
}

// ENTITY
QString AttributeEntity::getDataType()
{
	shared_ptr<Entity> ent( m_entity );
	return ent->m_entity_name;
}

QString AttributeEntity::getDataTypeWithSmartPointer(bool weak)
{
	shared_ptr<Entity> ent( m_entity );
	if(weak)
	{
		return "weak_ptr<" + ent->m_entity_name + ">";
	}
	else
	{
		return "shared_ptr<" + ent->m_entity_name + ">";
	}
}
