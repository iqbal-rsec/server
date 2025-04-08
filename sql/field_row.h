#ifndef FIELD_ROW_INCLUDED
#define FIELD_ROW_INCLUDED

#include "field_composite.h"

class Field_row final :public Field_composite
{
  class Virtual_tmp_table *m_table;
public:
  Field_row(uchar *ptr_arg, const LEX_CSTRING *field_name_arg)
    :Field_composite(ptr_arg, field_name_arg),
     m_table(NULL)
    {}
  ~Field_row();
  uint cols() const override;
  const Type_handler *type_handler() const override
  {
    return &type_handler_row;
  }
  void sql_type(String &str) const override;
  void sql_type_for_sp_returns(String &str) const override;
  en_fieldtype tmp_engine_column_type(bool use_packed_rows) const override
  {
    DBUG_ASSERT(0);
    return Field::tmp_engine_column_type(use_packed_rows);
  }
  enum_conv_type rpl_conv_type_from(const Conv_source &source,
                                    const Relay_log_info *rli,
                                    const Conv_param &param) const override
  {
    DBUG_ASSERT(0);
    return CONV_TYPE_IMPOSSIBLE;
  }
  virtual Virtual_tmp_table *virtual_tmp_table() const override
  {
    return m_table;
  }
  Virtual_tmp_table **virtual_tmp_table_addr() override { return &m_table; }
  bool row_create_fields(THD *thd, List<Spvar_definition> *list);
  bool row_create_fields(THD *thd, const Spvar_definition &def);
  bool sp_prepare_and_store_item(THD *thd, Item **value) override;

  Item_field *element_by_key(THD *thd, String *key) const override;
};


#endif /* FIELD_ROW_INCLUDED */