<?php
$default_theme="simple";

$this_file =
    strtolower(str_replace(".php", "", basename($PHP_SELF)));

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

setcookie("theme", $theme, time()+10*365*24*6300);
?>
