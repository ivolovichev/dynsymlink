# dynsymlink
FUSE Dynamic Symlink Support

It allows creating dynamic symbolic links to different locations
depending on calling process name, UIG/GID, etc. May be used to
customize system (e.g. GTK) on per application basis.

<h3><p>I. Installation</h3>
<h4>Prerequisites:</h4>
1. libfuse (<a href="https://github.com/libfuse/libfuse">https://github.com/libfuse/libfuse</a>)<p>
2. json-c (<a href="https://github.com/json-c/json-c">https://github.com/json-c/json-c</a>)<p>

<h4>Compiling:</h4>
1. Download sources to a directory.<br>
2. Execute “make” in it. Use “make CFLAGS=-DDLFS_PRINT_CONFIG” to enable code that prints
current configuration on load.<br>
3. Copy the binary “dlfs” to a convenient destination.

<h3>II. Configure</h3>
Create configuration file. It is in JSON format (RFC 8259). It is convenient
to use json online editor or chrome browser extension for this purpose. 
For human reader convenience, comments are partly implemented: 
line beginning with # (exactly in the first position!) is treated 
as a comment and is neglected by the standard JSON parser.<p>

The structure of the configuration file is as follow:<p>
<pre>
{
"<i>1st_symlink_name</i>":
     {
      "default":"<i>default_target_name</i>",
      "log":"<i>path_to_logfile</i>",                         &lt;---- OPTIONAL PARAMETER!
      "<i>1st_target_name</i>":{  <i>ACL</i>  },
      "<i>2nd_target_name</i>":{  <i>ACL</i>   },
                                ....
      },                          
"<i>2nd_symlink_name</i>":
     {
      "default":"<i>default_target_name</i>",
      "log":"<i>path_to_logfile</i>",                         &lt;---- <i>OPTIONAL PARAMETER!</i>
      "<i>1st_target_name</i>":{  <i>ACL</i>   },
#      "<i>2nd_target_name</i>":{  <i>ACL</i>   },            &lt;---- <i>LINE COMMENTED OUT!</i>
                                ...
      },                          
                                ...
}
</pre>
<p>Here  <i>default_target_name</i> is a path the symlink points to if no condition is met.
If not specified, /dev/null is used.
Optional parameter "log" specifies location of the logfile where all access to the symlink 
will be logged to. Tilde symbol (~) is not permitted here! <br>
ACL is the JSON object representing a condition. The dynamic symlink points to the first target 
with fulfilled condition. <p>

The following ACL are implemented.
<pre>"COMM":[  <i>array of command names</i>  ]</pre>
This ACL is met if the name of the process (as in /proc/<i>pid</i>/comm) accessing the symlink matches
any name in the specified array of strings.
<pre>"UID":[  <i>array of uid's</i>  ]
"GID":[  <i>array of gid's</i>  ]</pre>
These ACLs are met if the UID/GID of the process accessing the symlink matches
any item in the specified array of integers.
<pre>"EXTERN":"<i>command_line</i>"</pre>
This JSON object of the type "string" represents an external executable file "<i>command_line</i>".
If it's exit code is 0, the condition is met. Other exitcodes or inaccessible executable mean ACL is not fulfilled.
<pre>"NOT":{   <i>ACL</i>   }</pre>
Negates the enclosed ACL.
<pre>"ANY":{ <i>ACL1</i>, <i>ACL2</i>, ...  } 
"EVERY":{ <i>ACL1</i>, <i>ACL2</i>, ...  }</pre>
These ACLs enclose a coma-separated list of the above-mentioned ACL (i.e. JSON objects). 
The former is met if any of the enclosed ACL is met, the letter is met only if each of the enclosed ACL is met.
Nested expressions are permitted.
<p>Some configuration examples are in files example1.conf and example2.conf. 
File example1.conf shows usage of all above-mentioned ACLs. More realistic configuration is in example2.conf. 
It redirects GTK configuration (light/dark theme, etc) on per application basis 
(One should prepare appropriate versions of the GTK config files and create a symlink, redirecting original 
GTK config file to the dynamic symlink). In the following example applications putty, lxpanel and geeqie 
will use dark GTK theme regardless of the GTK version. All GTK2 accesses will be logged.

<h3>III. Usage</h3>
Shows help:
<pre>dlfs -h|--help</pre> 
Usage:
<pre>dlfs [<i>FUSE_options</i>] [-c | --config==<i>config_file</i>] <mount_point></pre> 
Default location of the configuration file is ~/.config/dynlink/config.
