DROP PACKAGE IF EXISTS utl_encode;

SET @old_sql_mode = @@session.sql_mode, @@session.sql_mode = oracle;

DELIMITER $$

CREATE DEFINER='mariadb.sys'@'localhost' PACKAGE utl_encode
  SQL SECURITY INVOKER
  COMMENT 'Character encoding and decoding functions'
  AS
    FUNCTION base64_decode(input RAW) RETURN RAW
      SQL SECURITY INVOKER
      COMMENT '
              Description
              -----------

              Decodes a base64 encoded string to a binary string. Raise `ER_STD_INVALID_ARGUMENT`
              if the input is NULL or empty.

              Parameters
              -----------

              input (RAW):
                The base64 encoded string to decode.
              
              Returns
              -------

                The decoded binary string (RAW). Returns NULL if the input
                cannot be decoded.
              '
    ;
END
$$

CREATE DEFINER='mariadb.sys'@'localhost' PACKAGE BODY utl_encode
  SQL SECURITY INVOKER
  AS
    FUNCTION base64_decode(input RAW) RETURN RAW
      SQL SECURITY INVOKER
      COMMENT '
          Description
          -----------

          Decodes a base64 encoded string to a binary string. Raise `ER_STD_INVALID_ARGUMENT`
          if the input is NULL or empty.

          Parameters
          -----------

          input (RAW):
            The base64 encoded string to decode. 
          
          Returns
          -------

            The decoded binary string (RAW). Returns NULL if the input
            cannot be decoded.
        '
    IS
    BEGIN
      IF input IS NULL OR LENGTH(input) = 0 THEN
        SIGNAL SQLSTATE 'HY000' set mysql_errno=3047, message_text='Invalid argument in function FROM_BASE64';
      END IF;

      -- Oracle is more lenient than MariaDB when it comes to padding.
      -- append padding if necessary
      IF MOD(LENGTHB(input), 4) <> 0 THEN
        RETURN CAST(FROM_BASE64(CAST(CONCAT(input, REPEAT('=', 4 - MOD(LENGTHB(input), 4))) AS CHAR)) AS BINARY);
      ELSE
        RETURN CAST(FROM_BASE64(CAST(input AS CHAR)) AS BINARY);
      END IF;
    END;
END
$$

DELIMITER ;

SET @@session.sql_mode = @old_sql_mode;
