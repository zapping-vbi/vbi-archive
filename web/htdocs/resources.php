<?php
require_once("php_common.inc");

switch ($theme)
{
 case "carsten":
  include($this_file . "_" . $theme . ".php");
  break;
 case "modern":
   header("Location: links.php#resources");
   break;
 default:
   header("Location: index.php#" . $this_file);
   break;
}

?>
