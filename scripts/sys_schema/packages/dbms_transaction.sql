DROP PACKAGE IF EXISTS dbms_transaction;

SET @old_sql_mode = @@session.sql_mode, @@session.sql_mode = oracle;

DELIMITER $$

CREATE DEFINER='mariadb.sys'@'localhost' PACKAGE dbms_transaction
  SQL SECURITY INVOKER
  COMMENT 'Collection of transaction management routines'
  AS
    PROCEDURE commit
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------
  
              Commits the current transaction.
  
              Raises
              ------
  
              ER_STD_INVALID_TRANSACTION_STATE if there is no active transaction.
              '
    ;
    PROCEDURE rollback
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------
  
              Rolls back the current transaction.
  
              Raises
              ------
  
              ER_STD_INVALID_TRANSACTION_STATE if there is no active transaction.
            '
    ;
END
$$

CREATE DEFINER='mariadb.sys'@'localhost' PACKAGE BODY dbms_transaction
  SQL SECURITY INVOKER
  AS
    PROCEDURE commit
      SQL SECURITY INVOKER
      COMMENT '
          Description
          -----------
  
          Commits the current transaction.
  
          Raises
          ------
  
          ER_STD_INVALID_TRANSACTION_STATE if there is no active transaction.
      '
    IS
    BEGIN
      COMMIT;
    END;

    PROCEDURE rollback
      SQL SECURITY INVOKER
      COMMENT '
          Description
          -----------
  
          Rolls back the current transaction.
  
          Raises
          ------
  
          ER_STD_INVALID_TRANSACTION_STATE if there is no active transaction.
      '
    IS
    BEGIN
      ROLLBACK;
    END;
END
$$

DELIMITER ;

SET @@session.sql_mode = @old_sql_mode;
