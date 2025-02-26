#ifndef DEBUG_PRINT_INCLUDED
#define DEBUG_PRINT_INCLUDED

#include "sql_list.h"
#include "sql_string.h"

struct TABLE;
class Item;
class THD;

class Debug_print
{
public:
  Debug_print()
    : m_table(nullptr)
  {}
  ~Debug_print();

  bool start(THD *thd);
  void stop();
  bool push(THD *thd, const Item &str);
  bool pop(THD *thd, String *line);
  bool pop(THD *thd, List<String> *lines);

protected:
  TABLE *m_table;
  THD *m_table_thd;
};

#endif /* DEBUG_PRINT_INCLUDED */