#include "sql_type.h"
#include "item.h"
#include "sql_type_assoc_array.h"
#include "field.h"
#include "sql_class.h"
#include "item.h"
#include "sql_select.h" // Virtual_tmp_table
#include "sp_rcontext.h"

class Type_collection_assoc_array: public Type_collection
{
public:
  bool init(Type_handler_data *data) override
  {
    return false;
  }
  const Type_handler *aggregate_for_result(const Type_handler *a,
                                           const Type_handler *b)
                                           const override
  {
    return NULL;
  }
  const Type_handler *aggregate_for_comparison(const Type_handler *a,
                                               const Type_handler *b)
                                               const override
  {
    DBUG_ASSERT(a == &type_handler_assoc_array);
    DBUG_ASSERT(b == &type_handler_assoc_array);
    return &type_handler_assoc_array;
  }
  const Type_handler *aggregate_for_min_max(const Type_handler *a,
                                            const Type_handler *b)
                                            const override
  {
    return NULL;
  }
  const Type_handler *aggregate_for_num_op(const Type_handler *a,
                                           const Type_handler *b)
                                           const override
  {
    return NULL;
  }
};


static Type_collection_assoc_array type_collection_assoc_array;


const Type_collection *Type_handler_assoc_array::type_collection() const
{
  return &type_collection_assoc_array;
}


const Type_handler *Type_handler_assoc_array::type_handler_for_comparison() const
{
  return &type_handler_assoc_array;
}


Field *Type_handler_assoc_array::
  make_table_field_from_def(TABLE_SHARE *share, MEM_ROOT *mem_root,
                            const LEX_CSTRING *name,
                            const Record_addr &rec, const Bit_addr &bit,
                            const Column_definition_attributes *attr,
                            uint32 flags) const
{
  DBUG_ASSERT(attr->length == 0);
  DBUG_ASSERT(f_maybe_null(attr->pack_flag));
  return new (mem_root) Field_assoc_array(rec.ptr(), name);
}


String *Type_handler_assoc_array::
          print_item_value(THD *thd, Item *item, String *str) const
{
  CHARSET_INFO *cs= thd->variables.character_set_client;
  StringBuffer<STRING_BUFFER_USUAL_SIZE> val(cs);
  String key;
  str->append(STRING_WITH_LEN("ASSOC_ARRAY("));

  auto composite= dynamic_cast<Item_composite_base *>(item);

  uint i= 0;
  if (!composite->get_key(&key, true))
  {
    do
    {
      if (i > 0)
        str->append(',');
      
      Item *elem= composite->element_by_key(thd, &key);
      String *tmp= elem->type_handler()->print_item_value(thd, elem, &val);
      if (tmp)
        str->append(*tmp);
      else
        str->append(NULL_clex_str);
      
      i++;
    } while (!composite->get_next_key(&key, &key));
  }
  else
    str->append(NULL_clex_str);

  str->append(')');
  return str;
}


class Func_handler_assoc_array_first:
        public Item_handled_func::Handler_str
{
public:
  const Type_handler *
      return_type_handler(const Item_handled_func *item) const override
  {
    return &type_handler_string;
  }


  bool fix_length_and_dec(Item_handled_func *) const override
  {
    return FALSE;
  }


  static Field_composite *get_field_assoc_array(Item *item)
  {
    Item_splocal* item_splocal= item->get_item_splocal();
    return dynamic_cast<Field_composite *>(item_splocal->this_item()->field_for_view_update()->field);
  }


  virtual String *val_str(Item_handled_func *item, String *tmp) const override
  {
    auto field_assoc_array= get_field_assoc_array(item->arguments()[0]);

    if (field_assoc_array->get_key(tmp, true)) {
      item->null_value= 1;
      return NULL;
    }

    item->null_value= 0;
    return tmp;
  }
};


class Func_handler_assoc_array_last:
        public Func_handler_assoc_array_first
{
  String *val_str(Item_handled_func *item, String *tmp) const override
  {
    auto field_assoc_array= get_field_assoc_array(item->arguments()[0]);

    if (field_assoc_array->get_key(tmp, false)) {
      item->null_value= 1;
      return NULL;
    }

    return tmp;
  }
};


class Func_handler_assoc_array_next:
        public Func_handler_assoc_array_first
{
  String *val_str(Item_handled_func *item, String *tmp) const override
  {
    DBUG_ASSERT(item->fixed());

    auto field= get_field_assoc_array(item->arguments()[0]);
    auto curr_key= item->arguments()[1]->val_str();

    if (field->get_next_key(curr_key, tmp)) {
      item->null_value= 1;
      return NULL;
    }

    return tmp;
  }
};


class Func_handler_assoc_array_prior:
        public Func_handler_assoc_array_first
{
  String *val_str(Item_handled_func *item, String *tmp) const override
  {
    DBUG_ASSERT(item->fixed());

    auto field= get_field_assoc_array(item->arguments()[0]);
    auto curr_key= item->arguments()[1]->val_str();

    if (field->get_prior_key(curr_key, tmp)) {
      item->null_value= 1;
      return NULL;
    }

    return tmp;
  }
};


class Func_handler_assoc_array_count:
        public Item_handled_func::Handler_ulonglong
{
  Longlong_null to_longlong_null(Item_handled_func *item) const override
  {
    DBUG_ASSERT(item->fixed());

    auto field_assoc_array=
      Func_handler_assoc_array_first::get_field_assoc_array(
        item->arguments()[0]);
    return Longlong_null(field_assoc_array->rows());
  }
};



/* Item_funcs for associative array methods */

class Item_func_assoc_array_first :public Item_handled_func
{
public:
  Item_func_assoc_array_first(THD *thd, Item *array)
    :Item_handled_func(thd, array) {}
  bool check_arguments() const override
  {
    return false;
  }
  LEX_CSTRING func_name_cstring() const override
  {
    static LEX_CSTRING name= {STRING_WITH_LEN("first") };
    return name;
  }
  bool fix_length_and_dec(THD *thd) override
  {
    static Func_handler_assoc_array_first ha_str_key;
    set_func_handler(&ha_str_key);
    return m_func_handler->fix_length_and_dec(this);
  }
  Item *do_get_copy(THD *thd) const override
  { return get_item_copy<Item_func_assoc_array_first>(thd, this); }
};


class Item_func_assoc_array_last :public Item_handled_func
{
public:
  Item_func_assoc_array_last(THD *thd, Item *array)
    :Item_handled_func(thd, array) {}
  bool check_arguments() const override
  {
    return false;
  }
  LEX_CSTRING func_name_cstring() const override
  {
    static LEX_CSTRING name= {STRING_WITH_LEN("last") };
    return name;
  }
  bool fix_length_and_dec(THD *thd) override;
  Item *do_get_copy(THD *thd) const override
  { return get_item_copy<Item_func_assoc_array_last>(thd, this); }
};


class Item_func_assoc_array_next :public Item_handled_func
{
public:
  Item_func_assoc_array_next(THD *thd, Item *array, Item *curr_key)
    :Item_handled_func(thd, array, curr_key) {}
  bool check_arguments() const override
  {
    return false;
  }
  LEX_CSTRING func_name_cstring() const override
  {
    static LEX_CSTRING name= {STRING_WITH_LEN("next") };
    return name;
  }
  bool fix_length_and_dec(THD *thd) override;
  Item *do_get_copy(THD *thd) const override
  { return get_item_copy<Item_func_assoc_array_next>(thd, this); }
};


class Item_func_assoc_array_prior :public Item_handled_func
{
public:
  Item_func_assoc_array_prior(THD *thd, Item *array, Item *curr_key)
    :Item_handled_func(thd, array, curr_key) {}
  bool check_arguments() const override
  {
    return false;
  }
  LEX_CSTRING func_name_cstring() const override
  {
    static LEX_CSTRING name= {STRING_WITH_LEN("prior") };
    return name;
  }
  bool fix_length_and_dec(THD *thd) override;
  Item *do_get_copy(THD *thd) const override
  { return get_item_copy<Item_func_assoc_array_prior>(thd, this); }
};


class Item_func_assoc_array_count :public Item_long_func
{
  bool check_arguments() const override
  { return arg_count != 1; }
public:
  Item_func_assoc_array_count(THD *thd, Item *array)
    :Item_long_func(thd, array) {}
  LEX_CSTRING func_name_cstring() const override
  {
    static LEX_CSTRING name= {STRING_WITH_LEN("count") };
    return name;
  }
  longlong val_int() override;
  bool fix_length_and_dec(THD *thd) override
  {
    decimals=0;
    max_length=1;
    set_maybe_null();
    return FALSE;
  }
  bool check_vcol_func_processor(void *arg) override
  {
    return mark_unsupported_function(func_name(), "()", arg, VCOL_IMPOSSIBLE);
  }
  Item *do_get_copy(THD *thd) const override
  { return get_item_copy<Item_func_assoc_array_count>(thd, this); }
};


class Item_func_assoc_array_exists :public Item_long_func
{
  bool check_arguments() const override
  { return arg_count != 2; }
  
public:
  Item_func_assoc_array_exists(THD *thd, Item *array, Item *key)
    :Item_long_func(thd, array, key) {}
  longlong val_int() override;
  LEX_CSTRING func_name_cstring() const override
  {
    static LEX_CSTRING name= {STRING_WITH_LEN("exists") };
    return name;
  }
  bool fix_length_and_dec(THD *thd) override
  {
    decimals=0;
    max_length=1;
    set_maybe_null();
    return FALSE;
  }
  bool check_vcol_func_processor(void *arg) override
  {
    return mark_unsupported_function(func_name(), "()", arg, VCOL_IMPOSSIBLE);
  }
  Item *do_get_copy(THD *thd) const override
  { return get_item_copy<Item_func_assoc_array_exists>(thd, this); }
};


class Item_func_assoc_array_delete :public Item_long_func
{
  bool check_arguments() const override
  { return arg_count < 1 || arg_count > 2; }
  
public:
  Item_func_assoc_array_delete(THD *thd, Item *array)
    :Item_long_func(thd, array)
  {}
  Item_func_assoc_array_delete(THD *thd, Item *array, Item *key)
    :Item_long_func(thd, array, key)
  {}
  longlong val_int() override;
  LEX_CSTRING func_name_cstring() const override
  {
    static LEX_CSTRING name= {STRING_WITH_LEN("exists") };
    return name;
  }
  bool fix_length_and_dec(THD *thd) override
  {
    decimals=0;
    max_length=1;
    set_maybe_null();
    return FALSE;
  }
  bool check_vcol_func_processor(void *arg) override
  {
    return mark_unsupported_function(func_name(), "()", arg, VCOL_IMPOSSIBLE);
  }
  Item *do_get_copy(THD *thd) const override
  { return get_item_copy<Item_func_assoc_array_delete>(thd, this); }
};


bool Item_func_assoc_array_last::fix_length_and_dec(THD *thd)
{
  static Func_handler_assoc_array_last ha_str_key;
  set_func_handler(&ha_str_key);
  return m_func_handler->fix_length_and_dec(this);
}


bool Item_func_assoc_array_next::fix_length_and_dec(THD *thd)
{
  static Func_handler_assoc_array_next ha_str_key;
  set_func_handler(&ha_str_key);
  return m_func_handler->fix_length_and_dec(this);
}


bool Item_func_assoc_array_prior::fix_length_and_dec(THD *thd)
{
  static Func_handler_assoc_array_prior ha_str_key;
  set_func_handler(&ha_str_key);
  return m_func_handler->fix_length_and_dec(this);
}


longlong Item_func_assoc_array_count::val_int()
{
  DBUG_ASSERT(fixed());
  DBUG_ASSERT(dynamic_cast<Item_composite_base *>(args[0]->this_item()));
  return dynamic_cast<Item_composite_base *>(args[0]->this_item())->rows();
}


longlong Item_func_assoc_array_exists::val_int()
{
  DBUG_ASSERT(fixed());
  DBUG_ASSERT(dynamic_cast<Item_composite_base *>(args[0]->this_item()));

  if (args[1]->null_value)
    return 0;

  return dynamic_cast<Item_composite_base *>(args[0]->this_item())->element_by_key(current_thd,
                                              args[1]->val_str()) != NULL;
}


longlong Item_func_assoc_array_delete::val_int()
{
  DBUG_ASSERT(fixed());

  auto field= dynamic_cast<Field_composite *>(args[0]->this_item()->field_for_view_update()->field);
  if (arg_count == 1)
    return field->delete_all_elements();
  else if (arg_count == 2)
    return field->delete_element_by_key(args[1]->val_str());

  return 0;
}


static
Item *sp_get_assoc_array_key(THD *thd, Item_splocal* array,
                                  List<Item> *args, bool is_first)
{
  DBUG_ASSERT(array);
  
  if (args)
  {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), is_first ? "FIRST" : "LAST", 
             "", 0, args->elements);
    return NULL;
  }

  return is_first ?
    (Item *)new (thd->mem_root) Item_func_assoc_array_first(thd, array) :
    (Item *)new (thd->mem_root) Item_func_assoc_array_last(thd, array);
}


static
Item *sp_get_assoc_array_next_or_prior(THD *thd,
                                            Item_splocal* array,
                                            List<Item> *args, bool is_next)
{
  DBUG_ASSERT(array);
  
  if (!args || args->elements != 1)
  {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), is_next ? "NEXT" : "PRIOR", 
             "", 1, args ? args->elements : 0);
    return NULL;
  }

  Item_args args_item(thd, *args);
  return is_next ? (Item *)
                      new (thd->mem_root)
                        Item_func_assoc_array_next(thd, array,
                                                   args_item.arguments()[0]) :
                   (Item *)
                      new (thd->mem_root)
                        Item_func_assoc_array_prior(thd, array,
                                                   args_item.arguments()[0]);
}


static
Item *sp_get_assoc_array_count(THD *thd, Item_splocal* array,
                                    List<Item> *args)
{
  DBUG_ASSERT(array);

  if (args)
  {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), "COUNT", 
             "", 0, args->elements);
    return NULL;
  }

  return new (thd->mem_root) Item_func_assoc_array_count(thd, array);
}


static
Item *sp_get_assoc_array_exists(THD *thd,
                                     Item_splocal* array,
                                     List<Item> *args)
{
  DBUG_ASSERT(array);

  if (!args || args->elements != 1)
  {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), "EXISTS", 
             "", 1, args ? args->elements : 0);
    return NULL;
  }

  Item_args args_item(thd, *args);
  return new (thd->mem_root)
            Item_func_assoc_array_exists(thd,
                                         array,
                                         args_item.arguments()[0]);
}


static
Item *sp_get_assoc_array_delete(THD *thd,
                                     Item_splocal* array,
                                     List<Item> *args)
{
  DBUG_ASSERT(array);

  if (args)
  {
    if (args->elements != 1)
    {
      my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), "DELETE", 
             "", 1, args->elements);
      return NULL;
    }

    Item_args args_item(thd, *args);
    return new (thd->mem_root)
      Item_func_assoc_array_delete(thd, array,
                                   args_item.arguments()[0]);
  }
  else
    return new (thd->mem_root) Item_func_assoc_array_delete(thd, array);
}


Item *Type_handler_assoc_array::create_item_method(THD *thd,
                                  Item_splocal *array,
                                  const Lex_ident_cli_st *method_name,
                                  List<Item> *args) const
{
  DBUG_ASSERT(method_name);

  Lex_ident_sys b(thd, method_name);
  if (b.length == 5)
  {
    if (Lex_ident_column(b).streq("COUNT"_Lex_ident_column))
      return sp_get_assoc_array_count(thd, array, args);
    else if (Lex_ident_column(b).streq("FIRST"_Lex_ident_column))
      return sp_get_assoc_array_key(thd, array, args, true); 
    else if (Lex_ident_column(b).streq("PRIOR"_Lex_ident_column))
      return sp_get_assoc_array_next_or_prior(thd, array, args, false);
  }
  else if (b.length == 4)
  {
    if (Lex_ident_column(b).streq("LAST"_Lex_ident_column))
      return sp_get_assoc_array_key(thd, array, args, false); 
    else if (Lex_ident_column(b).streq("NEXT"_Lex_ident_column))
      return sp_get_assoc_array_next_or_prior(thd, array, args, true);
  }
  else if (b.length == 6)
  {
    if (Lex_ident_column(b).streq("EXISTS"_Lex_ident_column))
      return sp_get_assoc_array_exists(thd, array, args);
    else if (Lex_ident_column(b).streq("DELETE"_Lex_ident_column))
      return sp_get_assoc_array_delete(thd, array, args);
  }

  my_error(ER_BAD_FIELD_ERROR, MYF(0), method_name->str);
  return NULL;
}


bool Type_handler_assoc_array::key_to_lex_cstring(THD *thd, Item **key, const LEX_CSTRING& name, LEX_CSTRING& out_key) const
{
  DBUG_ASSERT(key);
  DBUG_ASSERT(*key);

  if ((*key)->fix_fields_if_needed(thd, key))
    return true;
  
  if ((*key)->null_value)
  {
    my_error(ER_NULL_FOR_ASSOC_ARRAY_INDEX, MYF(0), name.str ? name.str : "unknown");
    return true;
  }

  auto str= (*key)->val_str();
  if (!str)
    return true;
  
  out_key= str->to_lex_cstring();
  return false;
}


Item_field *Type_handler_assoc_array::get_item(THD *thd, const Item_field *item, const LEX_CSTRING& name) const
{
  DBUG_ASSERT(item);
  
  auto item_assoc= dynamic_cast<const Item_field_assoc_array *>(item);
  if (!item_assoc)
    return nullptr;
  
  const Field_composite *field= item_assoc->get_composite_field();
  if (!field)
    return nullptr;

  // TODO modifiy element_by_key to accept LEX_CSTRING
  String key(name.str, name.length, &my_charset_bin);
  auto elem= field->element_by_key(thd, &key);
  if (!elem)
  {
    my_error(ER_ASSOC_ARRAY_ELEM_NOT_FOUND, MYF(0),
             name.str);
    return nullptr;
  }

  return elem;
}


Field *Type_handler_assoc_array::get_field(THD *thd, Item_field *item, const LEX_CSTRING& name) const
{
  DBUG_ASSERT(item);
  
  auto item_assoc= dynamic_cast<Item_field_assoc_array *>(item);
  if (!item_assoc)
    return nullptr;
  
  Field_composite *field= item_assoc->get_composite_field();
  if (!field)
    return nullptr;

  // TODO modifiy element_by_key to accept LEX_CSTRING
  String key(name.str, name.length, &my_charset_bin);
  Item_field *elem= field->element_by_key(thd, &key);
  if (!elem)
    return nullptr;
  
  return elem->field;
}


Field *Type_handler_assoc_array::get_field(THD *thd, const Item_field *item, const LEX_CSTRING& name) const
{
  Item_field *elem= get_item(thd, item, name);
  if (!elem)
    return nullptr;
  
  return elem->field;
}


bool Type_handler_row::get_item_index(THD *thd, const Item_field *item, const LEX_CSTRING& name, uint& idx) const
{
  auto item_row= dynamic_cast<Item_field_row *>(const_cast<Item_field *> (item));
  DBUG_ASSERT(item_row);

  auto vtable= item_row->field->virtual_tmp_table();
  if (!vtable)
    return true;
  
  return vtable->sp_find_field_by_name_or_error(&idx, item_row->field->field_name, name);
}


Item_field *Type_handler_row::get_item(THD *thd, const Item_field *item, const LEX_CSTRING& name) const
{
  auto item_row= dynamic_cast<Item_field_row *>(const_cast<Item_field *> (item));
  DBUG_ASSERT(item_row);

  uint field_idx;
  if (!get_item_index(thd, item_row, name, field_idx))
    return nullptr;

  return item_row->element_index(field_idx)->field_for_view_update();
}


Field *Type_handler_row::get_field(THD *thd, const Item_field *item, const LEX_CSTRING& name) const
{
  auto item_row= dynamic_cast<const Item_field_row *>(item);
  DBUG_ASSERT(item_row);

  auto vtable= item_row->field->virtual_tmp_table();
  DBUG_ASSERT(vtable);

  uint field_idx;
  if (vtable->sp_find_field_by_name_or_error(&field_idx, item_row->field->field_name, name))
    return nullptr;

  return vtable->field[field_idx];
}


/****************************************************************************
  Field_assoc_array, e.g. for associative array type SP variables
****************************************************************************/

/*
  The data structure used to store the key-value pairs in the
  associative array (TREE)
*/
struct Assoc_array_data :public Sql_alloc
{
  Assoc_array_data(String &&src_key, Item_field *value)
    : value(value)
  {
    key.swap(src_key);
  }
  Assoc_array_data(const String &src_key, Item_field *value)
    : key(src_key), value(value)
  {}

  String key;
  Item_field *value;
};


static int assoc_array_tree_cmp(void *arg, const void *lhs_arg,
                         const void *rhs_arg)
{
  const Assoc_array_data *lhs= (const Assoc_array_data *)lhs_arg;
  const Assoc_array_data *rhs= (const Assoc_array_data *)rhs_arg;
  return sortcmp(&lhs->key, &rhs->key, (CHARSET_INFO*)arg);
}


static int assoc_array_tree_del(void *data_arg, TREE_FREE, void*)
{
  DBUG_ASSERT(data_arg);
  Assoc_array_data *data= (Assoc_array_data *)data_arg;

  // Explicitly set the key's buffer to NULL to deallocate
  // the memory held in it's internal buffer.
  data->key.set((const char*)NULL, 0, &my_charset_bin); 
  return 0;
}


Field_assoc_array::Field_assoc_array(uchar *ptr_arg,
                                     const LEX_CSTRING *field_name_arg)
  :Field_composite(ptr_arg, field_name_arg),
   m_table(nullptr),
   m_def(nullptr),
   m_element_field(nullptr)
{
  init_alloc_root(PSI_NOT_INSTRUMENTED, &m_mem_root, 512, 0, MYF(0));

  m_table= (TABLE*) alloc_root(&m_mem_root, sizeof(TABLE)+ sizeof(TABLE_SHARE));
  if (!m_table)
    return;

  bzero((void *)m_table, sizeof(TABLE)+ sizeof(TABLE_SHARE));
  m_table->s= (TABLE_SHARE*) (m_table+1);

  m_table->alias.set("", 0, table_alias_charset);
  m_table->in_use= get_thd();
  m_table->copy_blobs= TRUE;
  m_table->s->table_cache_key= empty_clex_str;
  m_table->s->table_name= Lex_ident_table(empty_clex_str);
  
  init_tree(&m_tree, 0, 0,
            sizeof(Assoc_array_data), assoc_array_tree_cmp,
	          assoc_array_tree_del, NULL,
            MYF(MY_THREAD_SPECIFIC | MY_TREE_WITH_DELETE));
}


Field_assoc_array::~Field_assoc_array()
{
  m_table->alias.free();
  delete_tree(&m_tree, 0);

  free_root(&m_mem_root, MYF(0));
}


CHARSET_INFO *Field_assoc_array::key_charset() const
{
  if (!m_def || !m_def->elements)
    return &my_charset_bin;

  auto key_def= *m_def->begin();
  return key_def.charset;
}


bool Field_assoc_array::sp_prepare_and_store_item(THD *thd, Item **value)
{
  DBUG_ENTER("Field_assoc_array::sp_prepare_and_store_item");

  if (value[0]->type() == Item::NULL_ITEM)
  {
    delete_all_elements();

    DBUG_RETURN(false);
  }

  Item *src;
  if (!(src= thd->sp_fix_func_item(value)) ||
        src->cmp_type() != ROW_RESULT ||
        src->type_handler() != &type_handler_assoc_array)
  {
    my_error(ER_OPERAND_COLUMNS, MYF(0), m_table->s->fields);
    DBUG_RETURN(true);
  }

  src->bring_value();
  auto composite= dynamic_cast<Item_composite_base *>(src);

  delete_all_elements();

  Query_arena backup_arena;
  Query_arena owner_arena(&m_mem_root, Query_arena::STMT_INITIALIZED_FOR_SP);
  thd->set_n_backup_active_arena(&owner_arena, &backup_arena);

  String src_key;
  if (!composite->get_key(&src_key, true))
  {
    do
    {
      Item_field* element= create_element(thd);
      if (!element)
        goto error;

      Item **src_elem= composite->element_addr_by_key(thd, NULL, &src_key);
      if (!src_elem)
        goto error;

      if (element->field->sp_prepare_and_store_item(thd, src_elem))
        goto error;
      
      String key_copy;
      if (copy_and_convert_key(thd, &src_key, key_copy))
        goto error;

      if (insert_element(std::move(key_copy), element))
        goto error;

      set_notnull();
    } while (!composite->get_next_key(&src_key, &src_key));
  }

  thd->restore_active_arena(&owner_arena, &backup_arena);
  DBUG_RETURN(false);

error:
  thd->restore_active_arena(&owner_arena, &backup_arena);
  DBUG_RETURN(true);
}


bool Field_assoc_array::insert_element(String &&key, Item_field *element)
{
  Assoc_array_data data(std::move(key), element);
  
  if (unlikely(!tree_insert(&m_tree, &data, 0, (void *)key_charset())))
    return true;
  
  data.key.release();
  
  return false;
}


Item_field *Field_assoc_array::element_by_key(THD *thd, String *key)
{
  if (!key)
    return NULL;

  Item_field *item= NULL;

  Assoc_array_data key_data(*key, NULL);
  Assoc_array_data *data= (Assoc_array_data *)
                          tree_search(&m_tree, &key_data,
                          (void *)key_charset());
  if (data)
    item= data->value;

  Query_arena backup_arena;
  Query_arena owner_arena(&m_mem_root, Query_arena::STMT_INITIALIZED_FOR_SP);
  thd->set_n_backup_active_arena(&owner_arena, &backup_arena);
  if (!item)
  {
    // Create an element for the key if not found
    if (!(item= create_element(thd)))
      goto error;
    
    String key_copy;
    if (copy_and_convert_key(thd, key, key_copy))
      goto error;

    if (insert_element(std::move(key_copy), item))
      goto error;
    set_notnull();
  }

  thd->restore_active_arena(&owner_arena, &backup_arena);
  return item;

error:
  thd->restore_active_arena(&owner_arena, &backup_arena);
  return NULL;
}


Item_field *Field_assoc_array::element_by_key(THD *thd, String *key) const
{
  if (!key)
    return NULL;

  Item_field *item= NULL;

  String key_copy;
  if (copy_and_convert_key(thd, key, key_copy))
    return NULL;

  Assoc_array_data key_data(std::move(key_copy), NULL);
  Assoc_array_data *data= (Assoc_array_data *)
                           tree_search((TREE *)&m_tree,
                                        &key_data,
                                        (void *)key_charset());
  if (data)
    item= data->value;

  return item;
}


bool Field_assoc_array::copy_and_convert_key(THD *thd, const String *key, String &key_copy) const
{
  DBUG_ASSERT(key);

  uint errors;
  auto key_def= *m_def->begin();;
  if (key_def.type_handler()->field_type() == MYSQL_TYPE_VARCHAR)
  {
    if (key_copy.copy(key, key_def.charset, &errors))
      return true;

    if (key_copy.length() > key_def.length)
    {
      my_error(ER_TOO_LONG_KEY, MYF(0), key_def.length);
      return true;
    }
  }
  else
  {
    if (key_copy.copy(key, key_charset(), &errors))
      return true;

    // Convert the key to a number to perform range check
    // Follow Oracle's range for numerical keys
    char *endptr;
    int error;
    long key_long= key_charset()->strntol(key_copy.ptr(),
                                          key_copy.length(),
                                          10, &endptr, &error);

    if (error ||
        (endptr != key_copy.ptr() + key_copy.length()) ||
        key_long < INT32_MIN || key_long > INT32_MAX)
    {
      my_error(ER_WRONG_VALUE, MYF(0), "ASSOCIATIVE ARRAY KEY",
               key_copy.c_ptr());
      return true;
    }
  }

  return false;
}


class Field_assoc_array_element: public Field
{
protected:
  uchar *m_buffer;
  Field *m_field;

public:
  Field_assoc_array_element(uchar *buffer, Field *field)
    :Field(field->ptr, 0, field->null_ptr, field->null_bit,
	     field->unireg_check, &field->field_name),
     m_buffer(buffer), m_field(field)
  {
    init(field->table);
  }

  Virtual_tmp_table *virtual_tmp_table() const override
  {
    return m_field->virtual_tmp_table();
  }
  Virtual_tmp_table **virtual_tmp_table_addr() override
  {
    return m_field->virtual_tmp_table_addr();
  }

  virtual void read_from_buffer() const= 0;

  virtual void store_to_buffer()= 0;

  bool sp_prepare_and_store_item(THD *thd, Item **value) override
  {
    if (m_field->sp_prepare_and_store_item(thd, value))
      return true;

    store_to_buffer();

    return false;
  }

  Copy_func *get_copy_func(const Field *from) const override
  {
    return m_field->get_copy_func(from);
  }

  int save_in_field(Field *to) override
  {
    read_from_buffer();
    return m_field->save_in_field(to);
  }

  bool memcpy_field_possible(const Field *from) const override
  {
    //return m_field->memcpy_field_possible(from);
    return false;
  }

  bool send(Protocol *protocol) override
  {
    read_from_buffer();
    return m_field->send(protocol);
  }

  uint32 pack_length() const override
  {
    return m_field->pack_length();
  }

  int store(const char *to, size_t length,CHARSET_INFO *cs) override
  {
    if ( m_field->store(to, length, cs))
      return 1;
    
    store_to_buffer();
    return 0;
  }

  int store(double nr) override
  {
    if (m_field->store(nr))
      return 1;
    
    store_to_buffer();
    return 0;
  }

  int store(longlong nr, bool unsigned_val) override
  {
    if (m_field->store(nr, unsigned_val))
      return 1;
    
    store_to_buffer();
    return 0;
  }

  int store_decimal(const my_decimal *d) override
  {
    if (m_field->store_decimal(d))
      return 1;
    
    store_to_buffer();
    return 0;
  }

  double val_real() override
  {
    read_from_buffer();

    return m_field->val_real();
  }

  longlong val_int() override
  {
    read_from_buffer();

    return m_field->val_int();
  }

  bool val_bool() override
  {
    read_from_buffer();

    return m_field->val_bool();
  }

  my_decimal *val_decimal(my_decimal *dec) override
  {
    read_from_buffer();

    return m_field->val_decimal(dec);
  }

  String *val_str(String *val_buffer, String *val_ptr) override
  {
    read_from_buffer();

    return m_field->val_str(val_buffer, val_ptr);
  }

  const Type_handler *type_handler() const override
  {
    return m_field->type_handler();
  }

  enum_conv_type rpl_conv_type_from(const Conv_source &source,
                                            const Relay_log_info *rli,
                                            const Conv_param &param)
                                            const override
  {
    return m_field->rpl_conv_type_from(source, rli, param);
  }

  int cmp(const uchar *,const uchar *) const override
  {
    read_from_buffer();
    return m_field->cmp(m_buffer, m_field->ptr);
  }

  void sql_type(String &str) const override
  {
    m_field->sql_type(str);
  }

  uint size_of() const override
  {
    return m_field->size_of();
  }

  void sort_string(uchar *buff,uint length) override
  {
    read_from_buffer();
    m_field->sort_string(buff, length);
  }

  CHARSET_INFO *charset() const override
  {
    return m_field->charset();
  }

  const DTCollation &dtcollation() const override
  {
    return m_field->dtcollation();
  }

  uint32 max_display_length() const override
  {
    return m_field->max_display_length();
  }

  bool is_equal(const Column_definition &new_field) const override
  {
    return m_field->is_equal(new_field);
  }

  SEL_ARG *get_mm_leaf(RANGE_OPT_PARAM *param, KEY_PART *key_part,
                               const Item_bool_func *cond,
                               scalar_comparison_op op, Item *value) override
  {
    return m_field->get_mm_leaf(param, key_part, cond, op, value);
  }
};


class Field_assoc_array_scalar_element: public Field_assoc_array_element
{
public:
  Field_assoc_array_scalar_element(uchar *buffer, Field *field)
    :Field_assoc_array_element(buffer, field) {}
protected:
  void read_from_buffer() const override
  {
    m_field->unpack(m_field->ptr, m_buffer, m_buffer + 100);
  }

  void store_to_buffer() override
  {
    m_field->pack(m_buffer, m_field->ptr);
  }
};


class Field_assoc_array_row_element: public Field_assoc_array_element
{
public:
  Field_assoc_array_row_element(uchar *buffer, Field *field)
    :Field_assoc_array_element(buffer, field) {}
protected:
  void read_from_buffer() const override
  {
    auto ptable= virtual_tmp_table();
    const uchar *from= m_buffer;

    for (uint i= 0; i < ptable->s->fields; i++)
    {
      Field *field= ptable->field[i];
      from= field->unpack(field->ptr, from, m_buffer + 100);
    }
  }

  void store_to_buffer() override
  {
    auto ptable= virtual_tmp_table();
    auto to= m_buffer;

    for (uint i= 0; i < ptable->s->fields; i++)
    {
      Field *field= ptable->field[i];
      to= field->pack(to, field->ptr);
    }
  }
};


bool Field_assoc_array::init_element_field(THD *thd)
{
  auto value_def= *(++m_def->begin());
  if (value_def.is_column_type_ref())
  {
    Column_definition cdef;
    if (value_def.column_type_ref()->resolve_type_ref(thd, &cdef))
      return NULL;
    
    m_element_field= cdef.make_field(m_table->s, thd->mem_root, &empty_clex_str);
  }
  else
  {
    m_element_field= value_def.make_field(m_table->s, thd->mem_root, &empty_clex_str);
  }

  if (!m_element_field)
    return true;

  m_element_field->init(m_table);

  Field_row *field_row= dynamic_cast<Field_row*>(m_element_field);
  if (field_row)
  {
    field_row->field_name= field_name;
  }
  else
  {
    // Assign a buffer to the field
    uchar *tmp;
    if (!(tmp= (uchar *)thd->alloc(m_element_field->pack_length() + 1)))
      return true;
    m_element_field->move_field(tmp + 1, m_element_field->maybe_null() ? tmp : 0, 1);

    if (m_element_field->maybe_null())
      m_element_field->set_null();
    
    if (m_element_field->default_value)
      m_element_field->set_default();
  }

  return false;
}


Item_field *Field_assoc_array::create_element(THD *thd)
{
  Item_field *item= nullptr;

  if (!m_element_field)
  {
    if (init_element_field(thd))
      return nullptr;
    
    Field_row *field_row= dynamic_cast<Field_row*>(m_element_field);
    if (field_row)
    {
      auto value_def= *(++m_def->begin());
      // Modified from make_item_field_row
      if (!field_row->virtual_tmp_table())
      {
        if (field_row->row_create_fields(thd, value_def))
          return nullptr;
      }
    }
  }
  
  /* Allocate buffer for the proxy */
  //thd->alloc(m_element_field->pack_length() + 1);
  uchar *buffer= (uchar *)my_malloc(PSI_NOT_INSTRUMENTED, m_element_field->pack_length() + 1, MYF(MY_ZEROFILL | MY_WME)); 
  if (!buffer)
    return NULL;

  Field_row *field_row= dynamic_cast<Field_row*>(m_element_field);
  if (field_row)
  {
    // TODO this doesn't work since the fields within ROWs doesn't go through
    // the proxy
    auto temp= my_malloc(PSI_NOT_INSTRUMENTED, sizeof(Field_assoc_array_row_element), MYF(MY_ZEROFILL | MY_WME));
    if (!temp)
      return nullptr;
    auto field_proxy= ::new (temp) Field_assoc_array_row_element(buffer, m_element_field);
    if (!field_proxy)
      return nullptr;

    // Modified from make_item_field_row
    temp= my_malloc(PSI_NOT_INSTRUMENTED, sizeof(Item_field_row), MYF(MY_ZEROFILL | MY_WME));
    if (!temp)
      return nullptr;
    auto item_row= ::new (temp) Item_field_row(thd, field_proxy);
    if (!item_row)
      return nullptr;
    
    // field->virtual_tmp_table() returns nullptr in case of ROW TYPE OF cursor
    if (field_row->virtual_tmp_table() &&
        item_row->add_array_of_item_field(thd, *field_row->virtual_tmp_table()))
      return nullptr;
    
    item= item_row;
  }
  else
  {
    auto temp= my_malloc(PSI_NOT_INSTRUMENTED, sizeof(Field_assoc_array_scalar_element), MYF(MY_ZEROFILL | MY_WME));
    if (!temp)
      return nullptr;
    auto field_proxy= ::new (temp) Field_assoc_array_scalar_element(buffer, m_element_field);
    if (!field_proxy)
      return nullptr;
    
    temp= my_malloc(PSI_NOT_INSTRUMENTED, sizeof(Item_field), MYF(MY_ZEROFILL | MY_WME));
    if (!temp)
      return nullptr;
    item= ::new (temp) Item_field(thd, field_proxy);
  }

  return item;
}


Item **Field_assoc_array::element_addr_by_key(THD *thd, String *key)
{
  if (!key)
    return NULL;

  String key_copy;
  if (copy_and_convert_key(thd, key, key_copy))
    return NULL;

  Assoc_array_data key_data(key_copy, NULL);
  Assoc_array_data *data= (Assoc_array_data *)
                          tree_search(&m_tree,
                                      &key_data,
                                      (void *)key_charset());
  if (data)
    return (Item **)&data->value;

  return NULL;
}


bool Field_assoc_array::delete_all_elements()
{
  delete_tree(&m_tree, 0);
  set_null();
  return false;
}


bool Field_assoc_array::delete_element_by_key(String *key)
{
  if (!key)
    return false; // We do not care if the key is NULL

  Assoc_array_data key_data(*key, NULL);
  (void) tree_delete(&m_tree, &key_data, 0, (void *)key_charset());
  return false;
}


uint Field_assoc_array::rows() const
{
  return m_tree.elements_in_tree;
}


bool Field_assoc_array::get_key(String *key, bool is_first)
{
  TREE_ELEMENT **last_pos;
  TREE_ELEMENT *parents[MAX_TREE_HEIGHT+1];

  Assoc_array_data *data= (Assoc_array_data *)
                          tree_search_edge(&m_tree,
                                           parents,
                                           &last_pos, is_first ?
                                           offsetof(TREE_ELEMENT, left) :
                                           offsetof(TREE_ELEMENT, right));
  if (data)
  {
    key->copy(data->key);
    return false;
  }
  
  return true;
}


bool Field_assoc_array::get_next_key(const String *curr_key, String *next_key)
{
  DBUG_ASSERT(next_key);

  TREE_ELEMENT **last_pos;
  TREE_ELEMENT *parents[MAX_TREE_HEIGHT+1];

  if (!curr_key)
    return true;

  Assoc_array_data key_data(*curr_key, NULL);

  Assoc_array_data *data= (Assoc_array_data *)
                          tree_search_key(&m_tree, &key_data, 
                          parents, &last_pos,
                          HA_READ_AFTER_KEY, (void *)key_charset());
  if (data)
  {
    next_key->copy(data->key);
    return false;
  }
  return true;
}


bool Field_assoc_array::get_prior_key(const String *curr_key, String *prior_key)
{
  DBUG_ASSERT(prior_key);

  TREE_ELEMENT **last_pos;
  TREE_ELEMENT *parents[MAX_TREE_HEIGHT+1];

  if (!curr_key)
    return true;
  
  Assoc_array_data key_data(*curr_key, NULL);

  Assoc_array_data *data= (Assoc_array_data *)
                          tree_search_key(&m_tree,
                                          &key_data, 
                                          parents,
                                          &last_pos,
                                          HA_READ_BEFORE_KEY,
                                          (void *)key_charset());
  if (data)
  {
    prior_key->copy(data->key);
    return false;
  }
  return true;
}


bool Item_field_assoc_array::set_array_def(THD *thd,
                                           Row_definition_list *def)
{
  DBUG_ASSERT(field);

  m_def= def;
  Field_assoc_array *field_assoc_array= dynamic_cast<Field_assoc_array*>(field);
  if (!field_assoc_array)
    return true;

  field_assoc_array->set_array_def(def);
  return false;
}


uint Item_field_assoc_array::cols_for_elements() const
{
  auto value_def= *(++m_def->begin());
  if (value_def.is_row())
  {
    return value_def.row_field_definitions()->elements;
  }
  return 0;
}


bool Item_assoc_array::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed() == 0);
  null_value= 0;
  base_flags&= ~item_base_t::MAYBE_NULL;

  Item **arg, **arg_end;
  for (arg= args, arg_end= args + arg_count; arg != arg_end ; arg++)
  {
    if ((*arg)->fix_fields_if_needed(thd, arg))
      return TRUE;
    // we can't assign 'item' before, because fix_fields() can change arg
    Item *item= *arg;

    base_flags|= (item->base_flags & item_base_t::MAYBE_NULL);
    with_flags|= item->with_flags;
  }
  base_flags|= item_base_t::FIXED;
  return FALSE;
}


void Item_assoc_array::bring_value()
{
  for (uint i= 0; i < arg_count; i++)
    args[i]->bring_value();
}


void Item_assoc_array::print(String *str, enum_query_type query_type)
{
  str->append('(');
  for (uint i= 0; i < arg_count; i++)
  {
    if (i)
      str->append(',');
    args[i]->print(str, query_type);
    str->append('@');
    str->append(args[i]->name.str, args[i]->name.length);
  }
  str->append(')');
}


Item *Item_assoc_array::do_build_clone(THD *thd) const
{
  Item **copy_args= static_cast<Item **>
    (alloc_root(thd->mem_root, sizeof(Item *) * arg_count));
  if (unlikely(!copy_args))
    return 0;
  for (uint i= 0; i < arg_count; i++)
  {
    Item *arg_clone= args[i]->build_clone(thd);
    if (!arg_clone)
      return 0;
    copy_args[i]= arg_clone;
  }
  Item_assoc_array *copy= (Item_assoc_array *) get_copy(thd);
  if (unlikely(!copy))
    return 0;
  copy->args= copy_args;
  return copy;
}


uint Item_assoc_array::rows() const
{
  return arg_count;
}


bool Item_assoc_array::get_key(String *key, bool is_first)
{
  DBUG_ASSERT(key);

  uint current_arg;

  if (arg_count == 0)
    return true;

  if (is_first)
    current_arg= 0;
  else
    current_arg= arg_count - 1;

  key->set(args[current_arg]->name.str, args[current_arg]->name.length, &my_charset_bin);
  return false;
}


bool Item_assoc_array::get_next_key(const String *curr_key, String *next_key)
{
  DBUG_ASSERT(curr_key);
  DBUG_ASSERT(next_key);

  /*
    The code below is pretty slow, but a constructor is a one time operation
  */
  for (uint i= 0; i < arg_count; i++)
  {
    if (args[i]->name.length == curr_key->length() &&
        !memcmp(args[i]->name.str, curr_key->ptr(), curr_key->length()))
    {
      if (i == arg_count - 1)
        return true;
      next_key->set(args[i + 1]->name.str, args[i + 1]->name.length, &my_charset_bin);
      return false;
    }
  }

  return true;
}


Item *Item_assoc_array::element_by_key(THD *thd, String *key)
{
  DBUG_ASSERT(key);

  /*
    See the comment in get_next_key() about the performance
  */
  for (uint i= 0; i < arg_count; i++)
  {
    if (args[i]->name.length == key->length() &&
        !memcmp(args[i]->name.str, key->ptr(), key->length()))
      return args[i];
  }

  return NULL;
}


Item **Item_assoc_array::element_addr_by_key(THD *thd,
                                             Item **addr_arg,
                                             String *key)
{
  /*
    See the comment in get_next_key() about the performance
  */
  for (uint i= 0; i < arg_count; i++)
  {
    if (args[i]->name.length == key->length() &&
        !memcmp(args[i]->name.str, key->ptr(), key->length()))
      return &args[i];
  }

  return NULL;
}


Item_composite_base *Item_splocal_assoc_array_element::get_composite_variable(sp_rcontext *ctx) const
{
  return dynamic_cast<Item_composite_base *>(get_variable(ctx));
}


bool Item_splocal_assoc_array_element::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed() == 0);

  if (m_key->fix_fields_if_needed(thd, &m_key))
    return true;
  
  if (m_key->null_value)
  {
    my_error(ER_NULL_FOR_ASSOC_ARRAY_INDEX, MYF(0),
             m_name.str);
    return true;
  }

  Item *item= get_composite_variable(thd->spcont)->element_by_key(thd, m_key->val_str());
  if (!item)
  {
    my_error(ER_ASSOC_ARRAY_ELEM_NOT_FOUND, MYF(0),
             m_key->val_str()->ptr());
    return true;
  }

  set_handler(item->type_handler());
  return fix_fields_from_item(thd, ref, item);
}


Item *
Item_splocal_assoc_array_element::this_item()
{
  DBUG_ASSERT(m_sp == m_thd->spcont->m_sp);
  DBUG_ASSERT(fixed());
  DBUG_ASSERT(m_key->fixed());
  return get_composite_variable(m_thd->spcont)->element_by_key(m_thd, m_key->val_str());
}


const Item *
Item_splocal_assoc_array_element::this_item() const
{
  DBUG_ASSERT(m_sp == m_thd->spcont->m_sp);
  DBUG_ASSERT(fixed());
  DBUG_ASSERT(m_key->fixed());
  return get_composite_variable(m_thd->spcont)->element_by_key(m_thd, m_key->val_str());
}


Item **
Item_splocal_assoc_array_element::this_item_addr(THD *thd, Item **ref)
{
  DBUG_ASSERT(m_sp == thd->spcont->m_sp);
  DBUG_ASSERT(fixed());
  DBUG_ASSERT(m_key->fixed());
  return get_composite_variable(thd->spcont)->element_addr_by_key(m_thd, ref, m_key->val_str());
}


void Item_splocal_assoc_array_element::print(String *str, enum_query_type type)
{
  const LEX_CSTRING *prefix= m_rcontext_handler->get_name_prefix();
  str->append(prefix);
  str->append(&m_name);
  str->append('[');
  m_key->print(str, type);
  str->append(']');
  str->append('@');
  str->qs_append(m_var_idx);
  str->append('[');
  m_key->print(str, type);
  str->append(']');
}


bool Item_splocal_assoc_array_element::set_value(THD *thd, sp_rcontext *ctx, Item **it)
{
  LEX_CSTRING key;
  if (type_handler_assoc_array.key_to_lex_cstring(thd, &m_key, name, key))
    return true;

  return get_rcontext(ctx)->set_variable_composite_by_name(thd, m_var_idx, key,
                                                            it);
}


Item_composite_base *Item_splocal_assoc_array_element_field::get_composite_variable(sp_rcontext *ctx) const
{
  return dynamic_cast<Item_composite_base *>(get_variable(ctx));
}


bool Item_splocal_assoc_array_element_field::fix_fields(THD *thd, Item **ref)
{
  DBUG_ASSERT(fixed() == 0);

  if (m_key->fix_fields_if_needed(thd, &m_key))
    return true;

  Item *element_item_base= get_composite_variable(thd->spcont)->
                            element_by_key(thd, m_key->val_str());
  if (!element_item_base || !(m_element_item= element_item_base->field_for_view_update()))
  {
    my_error(ER_ASSOC_ARRAY_ELEM_NOT_FOUND, MYF(0),
             m_key->val_str()->c_ptr());
    return true;
  }

  auto element_handler= dynamic_cast<const Type_handler_composite *>(m_element_item->type_handler());
  if (!element_handler || element_handler->get_item_index(thd, m_element_item, m_field_name, m_field_idx))
  {
    my_error(ER_BAD_FIELD_ERROR, MYF(0),
             m_key->val_str()->c_ptr(), thd_where(thd));
    return true;
  }

  Item *item= m_element_item->element_index(m_field_idx);
  set_handler(item->type_handler());
  return fix_fields_from_item(thd, ref, item);
}


Item *
Item_splocal_assoc_array_element_field::this_item()
{
  DBUG_ASSERT(m_sp == m_thd->spcont->m_sp);
  DBUG_ASSERT(fixed());

  return m_element_item->element_index(m_field_idx);
}


const Item *
Item_splocal_assoc_array_element_field::this_item() const
{
  DBUG_ASSERT(m_sp == m_thd->spcont->m_sp);
  DBUG_ASSERT(fixed());

  return m_element_item->element_index(m_field_idx);
}


Item **
Item_splocal_assoc_array_element_field::this_item_addr(THD *thd, Item **)
{
  DBUG_ASSERT(m_sp == thd->spcont->m_sp);
  DBUG_ASSERT(fixed());

  return m_element_item->addr(m_field_idx);
}


void Item_splocal_assoc_array_element_field::print(String *str, enum_query_type type)
{
  const LEX_CSTRING *prefix= m_rcontext_handler->get_name_prefix();
  str->append(prefix);
  str->append(&m_name);
  str->append('[');
  m_key->print(str, type);
  str->append(']');
  str->append('.');
  str->append(&m_field_name);
  str->append('@');
  str->qs_append(m_var_idx);
  str->append('[');
  m_key->print(str, type);
  str->append(']');
  str->append('.');
  str->qs_append(m_field_idx);
}


bool Item_splocal_assoc_array_element::append_for_log(THD *thd, String *str)
{
  if (fix_fields_if_needed(thd, NULL))
    return true;

  if (limit_clause_param)
    return str->append_ulonglong(val_uint());

  if (str->append(STRING_WITH_LEN(" NAME_CONST('")) ||
      str->append(&m_name) ||
      str->append('[') ||
      ((m_key->val_str() && m_key->val_str()->ptr()) ?
        str->append(*m_key->val_str()) :
        str->append(NULL_clex_str)) ||
      str->append(']') ||
      str->append(STRING_WITH_LEN("',")))
    return true;
  return append_value_for_log(thd, str) || str->append(')');
}


bool Item_splocal_assoc_array_element_field::append_for_log(THD *thd,
                                                            String *str)
{
  if (fix_fields_if_needed(thd, NULL))
    return true;

  if (limit_clause_param)
    return str->append_ulonglong(val_uint());

  if (str->append(STRING_WITH_LEN(" NAME_CONST('")) ||
      str->append(&m_name) ||
      str->append('[') ||
      ((m_key->val_str() && m_key->val_str()->ptr()) ?
        str->append(*m_key->val_str()) :
        str->append(NULL_clex_str)) ||
      str->append(']') ||
      str->append('.') ||
      str->append(&m_field_name) ||
      str->append(STRING_WITH_LEN("',")))
    return true;
  return append_value_for_log(thd, str) || str->append(')');
}

