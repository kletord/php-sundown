--TEST--
Check for blockquote-multiline-2-paragraphs
--SKIPIF--
<?php if (!extension_loaded("sundown")) print "skip"; ?>
<?php if (!extension_loaded("tidy")) print "skip"; ?>
--FILE--
<?php
$data = <<< DATA
>A blockquote
>on multiple lines
>like this.
>
>But it has
>two paragraphs.
DATA;
$md = new Sundown\Markdown(new Sundown\Render\Html());
$result = $md->render($data);

$tidy = new tidy;
$tidy->parseString($result, array("show-body-only"=>1));
$tidy->cleanRepair();
echo (string)$tidy;
--EXPECT--
<blockquote>
<p>A blockquote on multiple lines like this.</p>
<p>But it has two paragraphs.</p>
</blockquote>
