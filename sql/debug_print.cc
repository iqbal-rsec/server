#include "mariadb.h"
#include "debug_print.h"
#include "sql_class.h"
#include "create_tmp_table.h"
#include "sql_select.h"

Debug_print::~Debug_print()
{
  stop();
}


void Debug_print::stop()
{
  if (m_table)
  {
    if (m_table->file->inited == handler::RND)
      m_table->file->ha_rnd_end();
    m_table->file->ha_delete_all_rows();

    free_tmp_table(m_table_thd, m_table);
    m_table= nullptr;
  }
}


bool Debug_print::start(THD *thd)
{
  if (m_table)
    return false;
  
  m_table_thd= thd;
  List<Item> temp_fields;

  Item_bin_string *item= new (thd->mem_root) Item_bin_string(thd, "55555", 5);;
  //item->max_length= 32767;
  item->max_length= 50;
  temp_fields.push_back(item, thd->mem_root);

  TMP_TABLE_PARAM tmp_param;
  tmp_param.init();
  tmp_param.tmp_name= "print";
  tmp_param.field_count= temp_fields.elements;
  tmp_param.func_count=  temp_fields.elements - 1;
  m_table= create_tmp_table(thd, &tmp_param, temp_fields,
                            (ORDER*) nullptr, 0, 0,
                            TMP_TABLE_ALL_COLUMNS, HA_POS_ERROR, &empty_clex_str);
  if (!m_table)
    return true;
  
  /* Ensure that we are using heap */
  DBUG_ASSERT(m_table->s->db_type() == heap_hton);
    
  return false;
}


bool Debug_print::push(THD *thd, const Item &value)
{
  int error;

  if (!m_table)
  {
    /* Silently ignore the messages */
    return false;
  }

  if (m_table->file->inited == handler::RND)
  {
    /* Reset the table */
    m_table->file->ha_rnd_end();
    m_table->file->ha_delete_all_rows();
  }

  List<Item> items;
  items.push_back((Item*) &value, thd->mem_root);
  fill_record(thd, m_table, m_table->field, items, true, true,
              true);
  if (unlikely(thd->is_error()))
    return true;
  
  if (unlikely((error=
                m_table->file->ha_write_tmp_row(m_table->record[0]))))
  {
    m_table->file->print_error(error, MYF(0));
    return true;
  }

  return false;
}


bool Debug_print::pop(THD *thd, String *line)
{
  int error;
  if (!m_table)
    return false;

  m_table->file->info(0);
  if (m_table->file->records() == 0)
  {
    return true;
  }

  if (m_table->file->inited != handler::RND &&
     (error= m_table->file->ha_rnd_init(true)))
  {
    m_table->file->print_error(error, MYF(0));
    return true;
  }

  if ((error= m_table->file->rnd_next(m_table->record[0])))
  {
    m_table->file->print_error(error, MYF(0));
    return true;
  }

  if (m_table->field[0]->val_str(line) == nullptr)
    return true;

  if ((error= m_table->file->ha_delete_tmp_row(m_table->record[0])))
  {
    m_table->file->print_error(error, MYF(0));
    return true;
  }

  return false;
}


bool Debug_print::pop(THD *thd, List<String> *lines)
{
  int error;
  if (!m_table)
  {
    return false;
  }

  m_table->file->info(0);
  if (m_table->file->records() == 0)
  {
    return true;
  }

  if (m_table->file->inited != handler::RND &&
     (error= m_table->file->ha_rnd_init(true)))
  {
    m_table->file->print_error(error, MYF(0));
    return true;
  }

  while (!(error= m_table->file->rnd_next(m_table->record[0])))
  {
    LEX_CSTRING tmp= m_table->field[0]->val_lex_string_strmake(thd->mem_root);
    String *line= new (thd->mem_root) String(tmp.str, tmp.length, &my_charset_bin);
    if (!line)
      return true;

    lines->push_back(line, thd->mem_root);

    if ((error= m_table->file->ha_delete_tmp_row(m_table->record[0])))
    {
      m_table->file->print_error(error, MYF(0));
      return true;
    }
  }

  return false;
}

