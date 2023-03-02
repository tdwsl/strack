\ strack test

MULTIPLES :
  0 BEGIN DUP 10 < WHILE
    1 + OVER OVER * . " " .
  REPEAT
  DROP DROP "\n" . ;

STRCHR :
  SWAP
  BEGIN
    # 1 - $: OVER OVER 1 :$
    = IF
      SWAP DROP EXIT
    THEN
    # 1 =
  UNTIL
  DROP DROP NIL ;

7 MULTIPLES

"Hello world!" " " STRCHR . "\n" .

