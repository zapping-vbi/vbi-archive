<!doctype html public "-//W3C//DTD HTML 4.01 Transitional//EN"
	"http://www.w3.org/TR/html4/loose.dtd">
<html>
  <!--
  Generated from index_simple.xml on Wed Mar 13 08:23:38 2002
  -->
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
    <title>Zapping source code</title>
<link REL="icon" HREF="/bookmark.ico" TYPE="image/png">
  </head>
<body bgcolor="#FFFFFF">
<center>
[ <a href="http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/~checkout~/zapping/zapping/ChangeLog?content-type=text/plain">ChangeLog</a> |
<a href="http://sourceforge.net/projects/zapping">Sourceforge
project page</a> |
<a href="screenshots.php">Screenshots</a> |
<a href="#contact">Contributing</a> ]
<p>
<img SRC="images_simple/logo.jpeg" ALT="[Zapping Logo]" height=168 width=436>
</p>
</center>
<center><h3>New release</h3></center><ul><li><b>Zapping</b> 0.6.3 

        <p>A new year, a new release, a new maintainer. This
	release does not include all improvements planned for 0.6.3,
	but it's over three months since the last release, without an
	end in sight. Future versions will be released at shorter
	intervals.</p><p>The main difference to 0.6.2 is the extraction of the
        Zapping vbi decoder into a separate package "zvbi".
        Motivation was to clean up and re-use the component in
        other applications, it may also encourage more frequent
	updates and reduce the size of binaries. Zapping can be
	compiled without zvbi, but a lot of functionality depends
	on the zvbi library, so you should really get it. The
	dependency is compiled into the Zapping	binary packages,
	so they need zvbi to install.</p><p>Other changes:</p><ul><li>Recording source and volume can be selected in
          preferences. Currently only the OSS /dev/mixer is
          supported. (And yes, ffmpeg support is underway.)</li><li>For our US and Canadian users Zapping now looks for
          XDS program title, start and end times, rating etc. and
          can display this information in the title bar or a
          program info window.</li><li>Complete rewrite of the screenshot plugin. Better
          dialog, supports JPEG and PPM (possibly more, inquire
          within), has quality preview and on-the-fly
          deinterlacing.</li><li>Some channel editor improvements.</li><li>The "snapshot button" present on ov511 web cams is
          scanned and does what it's supposed to.</li><li>Pointer disappears and reappears screen-saver
          style.</li><li>Some Teletext export improvements.</li><li>New mute audio toolbar button.</li><li>Channel numbers can be entered on the numeric keypad,
          either by channel memory number (European style) or RF
          channel number (US style).</li><li>Preliminary volume up/down function.</li><li>Most of the keybord shortcuts are configurable
          now.</li><li>And the usual set of bug fixes and new bugs.</li></ul><br><br></li><li><b>zvbi</b> 0.1 

        <p>First release of the Zapping vbi decoder as a standalone
        library.</p></li></ul><a href="download.php">Download instructions</a>.

<a name="index"></a><center><h2>General info</h2></center>
<center><h3>What is Zapping?</h3></center>

  Zapping is a TV viewer for the <a href="http://www.gnome.org">Gnome</a>
  environment. It has some interesting features, namely:

  <ul><li>It is for <a href="http://www.gnome.org">Gnome</a> /
      <a href="http://www.gtk.org">GTK</a>. This gives the program
      a very nice look, as you can see in the <a href="screenshots.php">screenshots</a> page.</li><li>It is based on plugins. This will make it easy to add
      functionality to the program, without making it hard to
      maintain, such as saving video as AVI/MPEG, viewing mirrored
      TV (scanning from right to left) or whatever you can imagine
      (and program).</li><li>Has a very advanced builtin VBI decoder, description <a href="Zapzilla.html">here</a>.</li><li>Lirc support.</li><li>Realtime MPEG recording.</li><li>And much more...</li></ul><a name="download"></a><center><h2>Downloading</h2></center>
<center><h3>Requirements for running Zapping</h3></center><ul><li>V4L, V4L2 or XVideo support for your TV card</li><li>A working Gnome desktop, with the common libraries</li><li>The <b>zvbi library</b>, available from the <a href="http://sourceforge.net/project/showfiles.php?group_id=2599">Zapping
      dowload page</a>.</li><li>libunicode version 0.4 or later</li><li>The remote control plugin (alirc) needs the LIRC (Linux
      Infrared Remote Control) package. If not already part of your
      distribution you can download LIRC from <a href="http://www.lirc.org">http://www.lirc.org</a>.</li><li>The mpeg plugin needs the <b>rte library</b>, available
      from the <a href="http://sourceforge.net/project/showfiles.php?group_id=2599">Zapping
      dowload page</a>. You need not download <b>mp1e</b> or
      <b>tveng</b>, these are already included in the rte library
      and Zapping, respectively.</li></ul><center><h3>RPM packages</h3></center>

    The RPMs on the Zapping dowload page have been built in the
    hope that they will be useful, but without any warranty. If you
    want to build your own packages, you can run the script
    <i>prepare_dist.sh</i> that comes with the zapping source: 
<pre>
tar xvIf zapping.tar.bz2
cd zapping
./prepare_dist.sh
</pre>
    I'm looking for volunteers to maintain packages for different
    distributions, if you are interested, please send an e-mail
    to the Zapping mailing list <a href="mailto:zapping-misc@lists.sourceforge.net">zapping-misc@lists.sourceforge.net</a>.
    All you will need to do is to type the lines above and send
    me the resulting package.

    <p>If some package dependency cannot be solved, a good way to
    install the missing packages is to <a href="http://www.ximian.com/download/">fetch</a> a <a href="http://www.ximian.com">Ximian</a> GNOME desktop. If you have
    an RPM based distribution, you can also look for the packages
    using <a href="http://www.rpmfind.net">RPMFIND</a>.</p><p>Debian users can get the debian packages from <a href="http://packages.debian.org/testing/graphics/zapping.html">testing</a>
    and <a href="http://packages.debian.org/unstable/graphics/zapping.html">unstable</a>,
    both maintained by Christian Marillat.</p><center><h3>Compiling Zapping</h3></center><p>If you want to compile Zapping yourself, make sure you have
    the appropiate development packages installed. For example, if
    compiling complains about AM_PATH_LIBGLADE, make sure you have
    libglade-devel correctly installed. See the INSTALL file for
    further details.</p><center><h3>Downloading via HTTP</h3></center><p>You can download the latest version of Zapping from the <a href="http://sourceforge.net/projects/zapping">SourceForge
    project page</a>, or all files released by this project from
    the <a href="http://sourceforge.net/project/showfiles.php?group_id=2599">project
    download page</a>.</p><p>Due to popular demand, the code interfacing with video
    capture drivers (V4L, V4L2 and XVideo) has been published as a
    separate package. The module called <b>tveng</b> is also
    available from the <a href="http://sourceforge.net/projects/zapping">project page.</a> The
    realtime MPEG-1 encoder <b>mp1e</b> the rte library is based
    on, is available as a stand-alone command line application from
    the same place.</p><a name="CVS"></a><center><h3>Downloading via CVS</h3></center><p>To get the latest development version of Zapping you can
    access the CVS server anonymously. <b>Remember there is no
    warranty that it will even compile.</b> This method is mainly
    for developers and people who want to have the very latest,
    probably very buggy, and occasionally very bugfixed, version.
    Enter the following:</p><ol><li><pre>
cvs -z3 -d:pserver:anonymous@cvs.zapping.sf.net:/cvsroot/zapping login
   
</pre>
        You will be asked for a password, just hit return.<br><br></li><li><pre>
cvs -z3 -d:pserver:anonymous@cvs.zapping.sf.net:/cvsroot/zapping co zapping

</pre>
        This will check out the latest version of Zapping (or "co
        rte", "co zvbi" if you want), from now on you can enter in
        your zapping (or rte or zvbi) directory:<br><br></li><li><pre>
cvs -z3 update -dP
</pre>
        to update your local copy of the CVS repository.
      </li></ol><br><br><p>Prior to the first compilation run ./autogen.sh which
    creates all missing and configuration files. automake 1.5 is
    required.</p><p>You can also <a href="http://cvs.sourceforge.net/cgi-bin/viewcvs.cgi/zapping">browse</a>
    the CVS repository, or download this daily updated snapshot of
    the CVS tree <a href="http://cvs.sourceforge.net/cvstarballs/zapping-cvsroot.tar.gz">
    zapping-cvsroot.tar.gz</a>. Caution: it contains the source
    and all changes ever made, the file is very large.</p><a name="contact"></a><center><h2>Contacting us</h2></center>
<center><h3>The mailing list</h3></center>
Please direct here anything related to the program, you don't need to be
subscribed to post. The address is <a href="mailto:zapping-misc@lists.sourceforge.net">
zapping-misc@lists.sourceforge.net</a>.
You can subscribe, search the archives, et al. at the
<a href="http://lists.sourceforge.net/lists/listinfo/zapping-misc">
mailing list page</a>.
<a name="bugs"></a><center><h2>Reporting bugs</h2></center>
<center><h3>What to do if you find a bug</h3></center><p>If you have any problems, please leave a bug report in the
    <a href="http://sourceforge.net/bugs/?group_id=2599">Bug
    tracker</a> or send an e-mail to the project <a href="http://lists.sourceforge.net/lists/listinfo/zapping-misc">mailing
    list</a>.</p><p>It should include:</p><ul><li>A description of the symptoms</li><li>How you triggered the bug</li><li>Causalities you observe, for example when the bug
      manifests by switching to channel 13 on Fridays, but not on
      other weekdays</li><li>When the problem is a program crash, more than the bare
        fact it is often useful to know <i>where</i> it happens.
	This information can be obtained with the GNU debugger,
	just type: 
<pre>
   you@yourbox> gdb zapping
   gdb> run [command line parameters, if any]
   -- crash --
   gdb> bt
</pre>
        (this prints the chain of functions called) 
<pre>
   gdb> quit
  
</pre></li></ul><br><p>I cannot promise a quick fix in all cases, but your effort
    is greatly appreciated and resolving the problem has top
    priority.</p><p>If this is your first bug report ever, you may want to read
    <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">How to
    Report Bugs Effectively</a> by Simon Tatham.</p><a name="links"></a><center><h2>Stuff we didn't write</h2></center>
<center><h3>TV programs for Linux</h3></center>
  There are more programs you can use to watch TV under Linux,
  here are the ones that I have tried and work for me:

  <ul><li>XawTV: <a href="http://bytesex.org/xawtv/">http://bytesex.org/xawtv/</a>.
      Very widespread app, with useful tools.</li><li>KWinTV: <a href="http://kwintv.sourceforge.net">http://kwintv.sourceforge.net</a>.
      KDE app, quite easy to use GUI.</li><li>AleVT: <a href="http://lecker.essen.de/~froese">http://lecker.essen.de/~froese</a>.
      Good TTX decoder, probably the second best around. ;-)</li></ul><center><h3>Developer info</h3></center><ul><li><a href="http://roadrunner.swansea.uk.linux.org/v4l.shtml">V4L</a>,
    the video interface present in stable kernels.</li><li><a href="http://www.thedirks.org/v4l2/">V4L2</a>, a new and better
    API for video.</li><li><a href="http://developer.gnome.org">developer.gnome.org</a>, the
    place for gnome programming info.</li><li><a href="http://www.rahul.net/kenton/xsites.framed.html">X11</a>.
    Tons of info about X programming.</li></ul><a name="resources"></a><center><h2>Stuff we wrote</h2></center>
<center><h3>What can be found in here</h3></center>
    Below are links or info about code we wrote that could be
    useful for other developers. Feel free to use this routines in
    your own programs, just drop a note to the author if you do so.

    <center><h3>RTE</h3></center><p>Real Time Encoder. Takes the extremely fast <a href="http://sourceforge.net/project/showfiles.php?group_id=2599">mp1e</a>
    encoding engine and exposes it through a simple to use and
    extensible API. This is a very good choice if you plan to add
    some sort of realtime compression to your project, but don't
    want to go through the mess of writing an encoder when there
    are already excellent choices out there. You just have to
    provide the audio/video data, RTE will take care of the rest.</p><p>This module is currently in transition to version 0.5,
    with a more robust and versatile API.</p><br><br><b>Downloading:</b> There are some packages released with each
    Zapping version, but since rte is heavily worked on, you are
    encouraged to check out the rte module from Zapping <a href="download.php#CVS">CVS</a>. 

    <center><h3>TVeng</h3></center>
    This is the abstraction layer between Zapping and the TV
    hardware. Supports V4L2, V4L and XVideo with a common API, you
    can think of it as TV capturing for dummies :) If you are
    thinking about playing with the TV device under Linux, you
    might find this interesting. <br><br><b>Downloading:</b> The latest tveng code comes with every
    Zapping release, and there are some standalone tveng packages
    too. The files are <i>src/tveng*</i>. 

    <center><h3>Unicode regular expressions</h3></center>
    We added support for UCS2 regular expressions in Zapzilla,
    adapted from Mark Leishner's URE package. Shouldn't be
    difficult to add UTF-8 support. <br><br><b>Downloading:</b> Comes with Zapping, the file named
    <i>common/ure.c</i>. 

    <center><h3>ZConf</h3></center>
    This is the configuration engine Zapping uses. Easy to use, it
    resembles the gconf api a bit, but much less powerful. Its main
    advantages are its robustness (i'm pretty confident it has no
    bugs) and its speed, once the configure tree is in memory
    there's little overhead in accessing the values. <br><br><b>Downloading:</b> Comes with Zapping, the file named
    <i>src/zconf.c</i>. 

    <center><h3>ZVBI library</h3></center>
    The Zapping/Zapzilla vbi decoding routines, now a standalone
    library. Some may call this a libc of vbi.<br><br><b>Downloading:</b> See the Zapping download page. <br><br><p>I would like to thank <a href="http://sourceforge.net"><img
SRC="http://sourceforge.net/sflogo.php?group_id=2599&amp;type=1"
BORDER=0 height=31 width=88 ALT="Sourceforge Logo"></a>
for hosting this site and supporting Open Software
Concept. <b>Thanks</b>.<p>
<center>
<a href="http://validator.w3.org/check/referer"><img border="0"
src="valid-html401.gif" alt="Valid HTML 4.01" height=31 width="88"></a>
<a href="rescd.html"><img src="images_simple/gtcd.png" border="0"
ALT="ResCD"></a>
</center>
<div align="right">
<form action="<?php echo $PHP_SELF ?>" method="POST">
<select name="sel_theme" title="Select your theme here">
<option value="simple" <?php if ($theme=="simple") echo "selected" ?>>Simple</option>
<option value="carsten" <?php if ($theme=="carsten") echo "selected" ?>>Carsten</option>
<option value="modern" <?php if ($theme=="modern") echo "selected" ?>>Modern</option>
</select><input type="submit" name="Submit" value="Select Theme">
</form>
</div>
</body>
</html>
