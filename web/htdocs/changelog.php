<?php
require_once("php_common.inc");

# Tell the browser that it's seeing UTF-8
header("Content-Type: text/html; charset=UTF-8");

switch ($theme)
{
 case "carsten":
 case "modern":
  include($this_file . "_" . $theme . ".php");
  break;
 default:
   redirect("index.php#" . $this_file);
   break;
}

?>
