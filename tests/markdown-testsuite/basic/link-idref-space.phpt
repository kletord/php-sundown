--TEST--
Check for link-idref-space
--SKIPIF--
<?php if (!extension_loaded("sundown")) print "skip"; ?>
<?php if (!extension_loaded("tidy")) print "skip"; ?>
--FILE--
<?php
$data = <<< DATA
[World Wide Web Consortium] [w3c]

[w3c]: http://www.w3.org/
DATA;
$md = new Sundown($data);
$result = $md->toHtml();

$tidy = new tidy;
$tidy->parseString($result, array("show-body-only"=>1));
$tidy->cleanRepair();
echo (string)$tidy;
--EXPECT--
<p><a href="http://www.w3.org/">World Wide Web Consortium</a></p>
