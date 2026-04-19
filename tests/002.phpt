--TEST--
test1() Basic test
--EXTENSIONS--
lux
--FILE--
<?php
$ret = test1();

var_dump($ret);
?>
--EXPECT--
The extension lux is loaded and working!
NULL
