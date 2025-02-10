DROP PACKAGE IF EXISTS utl_raw;

SET @old_sql_mode = @@session.sql_mode, @@session.sql_mode = oracle;

DELIMITER $$

CREATE DEFINER='mariadb.sys'@'localhost' PACKAGE utl_raw
  SQL SECURITY INVOKER
  COMMENT 'Collection of routines to manipulate RAW data'
  AS
    FUNCTION cast_to_varchar2(input RAW) RETURN VARCHAR2
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------

              Converts RAW data to a VARCHAR2 string.

              Parameters
              -----------

              input (RAW):
                The RAW data
              
              Returns
              -------

                The converted VARCHAR2 string. Returns NULL if input is NULL.
              '
    ;
    FUNCTION cast_to_nvarchar2(input RAW) RETURN NVARCHAR
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------

              Converts RAW data to a NVARCHAR string.

              Parameters
              -----------

              input (RAW):
                The RAW data
              
              Returns
              -------

                The converted NVARCHAR string. Returns NULL if input is NULL.
              '
    ;
    FUNCTION cast_to_raw(input VARCHAR2) RETURN RAW
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------

              Converts a VARCHAR2 string to RAW data.

              Parameters
              -----------

              input (VARCHAR2):
                The VARCHAR2 string
              
              Returns
              -------

                The converted RAW data. Returns NULL if input is NULL.
              '
    ;
END
$$

CREATE DEFINER='mariadb.sys'@'localhost' PACKAGE BODY utl_raw
  SQL SECURITY INVOKER
  AS
    FUNCTION cast_to_varchar2(input RAW) RETURN VARCHAR2
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------

              Converts RAW data to a VARCHAR2 string.

              Parameters
              -----------

              input (RAW):
                The RAW data
              
              Returns
              -------

                The converted VARCHAR2 string. Returns NULL if input is NULL.
              '
    IS
    BEGIN
      RETURN CAST(input AS VARCHAR2);
    END;

    FUNCTION cast_to_nvarchar2(input RAW) RETURN NVARCHAR
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------

              Converts RAW data to a NVARCHAR string.

              Parameters
              -----------

              input (RAW):
                The RAW data
              
              Returns
              -------

                The converted NVARCHAR string. Returns NULL if input is NULL.
        '
    IS
    BEGIN
      RETURN CAST(input AS VARCHAR2);
    END;

    FUNCTION cast_to_raw(input VARCHAR2) RETURN RAW
      SQL SECURITY INVOKER
      COMMENT '
          Description
          -----------

          Converts a VARCHAR2 string to RAW data.

          Parameters
          -----------

          input (VARCHAR2):
            The VARCHAR2 string
          
          Returns
          -------

            The converted RAW data. Returns NULL if input is NULL.
        '
    IS
    BEGIN
      RETURN CAST(input AS BINARY);
    END;
END
$$

DELIMITER ;

SET @@session.sql_mode = @old_sql_mode;
