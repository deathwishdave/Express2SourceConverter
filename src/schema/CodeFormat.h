/* -*-c++-*- IFC++ www.ifcquery.com  Copyright (c) 2017 Fabian Gerold */

#include <QtCore/QString>
#include <assert.h>
#include <set>
#include <sstream>

class CodeFormat
{
public:
	static std::set<QString> m_files_exit_replaced;
	static QStringList m_files_with_exit_calls;

	static void format_indentation( const char* source, std::stringstream& target )
	{
		char* pos_begin = (char*)source;
		char* pos = (char*)source;
		char* pos_line_begin= pos;
		char* last_pos = pos;
		if( !pos )
		{
			return;
		}

		//insert new indentations according to the number of open curly braces
		int num_opening_braces = 0;
		//int current_line_minus = 0;
		while(*pos != '\0')
		{
			if( *pos == '\n' )
			{
				++pos;
				if( *pos == '\0' )
				{
					break;
				}

				pos_line_begin = pos;

				int minus = 0;
				if( *last_pos == 'p' )
				{
					if( strncmp( last_pos, "public:",7 )== 0 )
					{
						minus=1;
					}
					else if( strncmp( last_pos, "private:",8 )== 0 )
					{
						minus=1;
					}
					else if( strncmp( last_pos, "protected:",10 )== 0 )
					{
						minus=1;
					}
				}
				else if( *last_pos == '{' )
				{
					int chars_between_braces = pos - last_pos;
					if( chars_between_braces > 0 )
					{
						minus = 1;
					}
				}

				if( pos_line_begin > last_pos )
				{
					// insert tabs
					for( int i=0; i<num_opening_braces-minus; ++i ) { target << "\t"; }

					std::string current_line( last_pos,pos_line_begin-last_pos);
					target << current_line;

					while(isspace(*pos_line_begin))
					{
						if( *pos_line_begin == '\n')
						{
							target << "\n";
						}
						++pos_line_begin;
					}
					last_pos = pos_line_begin;
					
				}
				else
				{
					// empty line
					while(*pos_line_begin=='\t' || *pos_line_begin==' '){
						++pos_line_begin;
					}
					last_pos = pos_line_begin;

				}

				if( *pos == '{' )
				{
					++num_opening_braces;
					//current_line_minus = 1;
				}
				else if( *pos == '}' )
				{
					--num_opening_braces;
				}
				

			}
			else if( *pos == '{' )
			{
				++num_opening_braces;
				//current_line_minus = 1;
			}
			else if( *pos == '}' )
			{
				if( pos != pos_begin )
				{
					char* pos_next = pos+1;
					if( pos_next == nullptr )
					{
						break;
					}
					if( *pos_next == ';')
					{
						// insert tabs
						for( int i=0; i<num_opening_braces; ++i ) { target << "\t"; }
						//current_line_minus = 0;

						std::string current_line( last_pos,pos-last_pos);
						last_pos = pos;
						target << current_line;
						target << "\n";
					}
				}

				--num_opening_braces;
			}
			++pos;
		}
		std::string current_line( last_pos,pos-last_pos);
		target << current_line;
		return;
	}
};
