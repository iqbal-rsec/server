DROP PACKAGE IF EXISTS dbms_output;

SET @old_sql_mode = @@session.sql_mode, @@session.sql_mode = oracle;

DELIMITER $$

CREATE DEFINER='mariadb.sys'@'localhost' PACKAGE dbms_output
  SQL SECURITY INVOKER
  COMMENT 'Collection of routines to manage output'
  AS
    PROCEDURE put_line(input TEXT(32767))
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------
  
              Writes a line of text to the output buffer.

              Raises
              ------

              ER_STD_INVALID_ARGUMENT if the input string is NULL.
              '
    ;
    PROCEDURE get_line(line OUT TEXT(32767), status OUT BOOLEAN)
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------
  
              Retrieves a line of text from the output buffer.
              '
    ;
    PROCEDURE enable(buffer_size IN INTEGER DEFAULT 20000)
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------
  
              Enables the output buffer.
              '
    ;
END
$$

CREATE DEFINER='mariadb.sys'@'localhost' PACKAGE BODY dbms_output
  SQL SECURITY INVOKER
  AS
    PROCEDURE put_line(input TEXT(32767))
      SQL SECURITY INVOKER
    IS
    BEGIN
      PRINT input;
    END;

    PROCEDURE get_line(line OUT TEXT(32767), status OUT BOOLEAN)
      SQL SECURITY INVOKER
    IS
    BEGIN
      SET line = '';
      SET status = FALSE;
    END;
    PROCEDURE enable(buffer_size IN INTEGER DEFAULT 20000)
      SQL SECURITY INVOKER
    IS
    BEGIN
      NULL;
    END;
END
$$

DELIMITER ;

SET @@session.sql_mode = @old_sql_mode;
