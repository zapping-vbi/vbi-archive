<?php
$default_theme="simple";

$this_file =
    strtolower(str_replace(".php", "", basename($PHP_SELF)));

$this_dir = dirname($PHP_SELF);
if ($this_dir == "/")
{
	$this_dir = "";
}

if (!$theme)
{
	$theme = $default_theme;
}

function redirect($url)
{
	global $this_dir;
	global $SERVER_NAME;
	header("Location: http://" . $SERVER_NAME . $this_dir . "/" . $url);
}

switch ($sel_theme)
{
case "simple":
case "carsten":
case "modern":
    $theme = $sel_theme;
    break;
default:
    break;
}

setcookie("theme", $theme, time()+10*365*24*6300);
?>
