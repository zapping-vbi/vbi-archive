<?php
// Configure cookie behaviour.
session_set_cookie_params(10*365*24*3600);

session_start();

$default_theme="simple";
$this_file =
    strtolower(str_replace(".php", "", basename($PHP_SELF)));

if ((!$PHPSESSID) || (!$theme)) {
    session_register('theme');
}

if (!$theme) {
  $theme = $default_theme;
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

include($this_file . "_" . $theme . ".php");

?>
