<?php
require("php_common.inc");

switch ($theme)
{
 case "carsten":
 case "modern":
  include($this_file . "_" . $theme . ".php");
  break;
 default:
   header("Location: index.php#" . $this_file);
   break;
}

?>
