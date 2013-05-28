### vim: set ft=ruby ts=4 sw=4:
#
# Copyright © 2013 Midgard Serpent. All Rights Reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
# FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

$,=' '

#default options
OPTS_DEF={
	'MKBUILDDIRS'=>true,	#generate bin and obj directories
	'PERL'=>false,
	'INSTALDOCS'=>true,		# whether to install documentation
	'BITS'=>64,				# TODO: by default use native BITS settings: use RbConfig['host_cpu']
}
OPTS_DEF.each_key{|opt|
	if not OPTS.has_key?(opt)
		OPTS[opt]=OPTS_DEF[opt];
	end
}
BITS=OPTS['BITS']

BINDIR='bin/'+RUBY_PLATFORM+'/';
OBJDIR='obj/'+RUBY_PLATFORM+'/';
CPP_FLAGS||=["-O2 -m#{BITS}"];
$CPP_FLAGS=CPP_FLAGS.map{|v| v.chomp}.join(' ')
ASM_FLAGS||=[];
$ASM_FLAGS=ASM_FLAGS.map{|v| v.chomp}.join(' ')

DEBUG=ENV['DEBUG'].to_i;
SourceFlags||={};
Install||=[];
DefTasks=[];
OPTS||={};
DEFS=[];

CC={	#compiler configuration
	:cpp=>{	#C++
		:cc=>'g++'
	},
	:c=>{	#C
		:cc=>'gcc'
	},
	:asm=>{	#asm
		:cc=>'nasm'
	}
}

#options for tests
TESTOPTS={
	'.c'=>"-Os -s -I./ -L#{BINDIR}"
}

$LINK_FLAGS=[]

rapp=Rake::Application.new;

#platform dependend variables
if rapp.windows?
	DEFS.push '-DPLAT_WINDOWS'
	BINEXT='.exe'
	INSTALL_BASE_DIR="C:/Program Files/#{PROJECTNAME}"
	INSTALL_DIR={
		#~ :base=>INSTALL_BASE_DIR,
		:exec=>INSTALL_BASE_DIR+'/bin',
		:shared=>nil,
		:dll=>INSTALL_BASE_DIR+'/lib',
		:static=>INSTALL_BASE_DIR+'/lib',
		:header=>INSTALL_BASE_DIR+'/include',
		#:ruby_lib=>nil,
  	}
	ASM_OBJ="win#{BITS}"	#output format for .asm files
elsif rapp.unix?
	DEFS.push '-DPLAT_UNIX'
	BINEXT=''
	if RUBY_PLATFORM=~/(net|free|open)bsd/i then
		INSTALL_BASE_DIR='/usr/local'
		$CPP_FLAGS+=' -I /usr/local/include'
		$LINK_FLAGS.push ' -L /usr/local/lib'
	else
		INSTALL_BASE_DIR='/usr'
		$LINK_FLAGS.push ' -L /usr/lib'
	end
	INSTALL_DIR={
		#~ :base=>INSTALL_BASE_DIR,
		:exec=>INSTALL_BASE_DIR+'/bin',
		:shared=>INSTALL_BASE_DIR+'/lib',
		:dll=>nil,
		:static=>INSTALL_BASE_DIR+'/lib',
		:header=>INSTALL_BASE_DIR+'/include',
		#:ruby_lib=>INSTALL_BASE_DIR+"/lib/ruby/#{RUBY_VERSION.to_f}",	#DEPRECATED
	}
	ASM_OBJ="elf#{BITS}"
end
#platform independent vars
require 'rbconfig'
INSTALL_DIR[:ruby_module]=RbConfig::CONFIG['sitelibdir']
INSTALL_DIR[:man]=RbConfig::CONFIG['mandir']
if OPTS['PERL'] then
	INSTALL_DIR[:perl_module]=%x[perl -MConfig -e 'print $Config{installsitelib}']
end
#
if DEBUG>0 then
	$LINK_FLAGS.push " -lXDebug -m#{BITS}"
	DEFS.push "-DXDEBUG=#{DEBUG}"
else
	$LINK_FLAGS.push " -s -m#{BITS}";
end

$DEFS=DEFS.join(' ')
LINK_FLAGS=$LINK_FLAGS.join(' ')

#install
def installTask(task, taskname)
  files=[];
  if task[:install].nil? then
    case task[:type]
      when :exec then
	files.push({:src=>task[:output], :dst=>INSTALL_DIR[:exec], :opts=>{:mode=>0755}})
      when :shared then
	files.push({:src=>task[:output], :dst=>INSTALL_DIR[:shared], :opts=>{}})
      when :dll then
	files.push({:src=>task[:output], :dst=>INSTALL_DIR[:dll], :opts=>{}})
      when :static then
	files.push({:src=>task[:output], :dst=>INSTALL_DIR[:static], :opts=>{}})
	if not task[:headers].empty? then	#???
	  mkdir_p INSTALL_DIR[:header]+"/#{taskname}"
	  task[:headers].each{|h|
	    files.push({:src=>h, :dst=>INSTALL_DIR[:header]+"/#{taskname}", :opts=>{}})
	  }
	end
      else return -1;
    end
  else
    files.push({:src=>task[:output], :dst=>task[:install][:dst], :opts=>task[:install][:opts]})
  end
  print "==>> installing #{taskname}\n"
  files.each{|f|
    installFile f[:src], f[:dst], f[:opts]
    }
end
# install contents of given directory src in directory dst
def installDirContent(src, dst, opts ={})
  if not test(?d, dst)
    mkdir_p dst;	#TODO: change owner,group and permissions?
  end
  FileList.new("#{src}/*").each do |file|
    installFile file, dst, opts;
  end
end
# install given directory src (recursive) in directory dst
def installDir(src, dst, opts ={})
  dest="#{dst}/#{split_all(src)[-1]}";
  if not test(?d, dest)
    mkdir_p dest;	#TODO: change owner,group and permissions?
  end
  #FileList.new("#{src}/*").each do |file|
    #installFile file, dest, opts;
  #end
	installDirContent(src, dest, opts);
end
# install file/directory(recursive) src in directory dst
def installFile(src, dst, opts ={})
  print "install #{src} => #{dst}\n"
  if test(?d, src)
    installDir(src, dst, opts);
  elsif test(?f, src)
    FileUtils.install src, dst, opts;
  else
    raise "Don't know how to install #{src}: it's neither regular file nor directory\n";
  end
end
# install file src of known type
def installFile_Type(src, type, opts ={})
  if INSTALL_DIR.has_key?(type)
    installFile src, INSTALL_DIR[type], opts
  else
    raise "Don't know how to install file '#{src}' of type '#{type}'"
  end
end
# install man pages
def installDocs_Man()
  print "installing man pages\n"
	installDirContent('doc/man', INSTALL_DIR[:man], {:mode=>0444});
end
# install documentation
def installDocs()
	if OPTS['INSTALLDOCS']
		if test(?d, 'doc/man')
			installDocs_Man();
		end
	end
end

Tasks.each_key do |t|
  if Tasks[t][:type].nil? then
    Tasks[t][:type]=:exec
  end
  if not Tasks[t].has_key?(:sources)
    Tasks[t][:sources]=[];
  end
  if not Tasks[t].has_key?(:link_flags)
    Tasks[t][:link_flags]=[];
  end
  #
  tt= DEBUG>0 ? "#{t}-DEBUG" : t;
  name= case Tasks[t][:type]
    when :exec then "#{tt+BINEXT}"
    when :shared then "#{tt}.so"
    when :dll then "#{tt}.dll"
    when :static then "lib#{tt}.a"
    when :perl_module then "#{tt}.pm"
    else tt
  end
  Tasks[t][:name]=tt;
  DefTasks.push(Tasks[t][:output]=BINDIR+name);
  ext= DEBUG>0 ? '-DEBUG.o' : '.o'
  Tasks[t][:obj]=Tasks[t][:sources].map{|s|
    OBJDIR+s.sub(/\.\w+$/, ext)	#String::ext does not work with DEBUG mode
  }
  Tasks[t][:link_flags]=Tasks[t][:link_flags].map{|v| v.chomp}.join(' ')
end

task :default=>[:dirs, :compile, :test]

desc 'compile sources'
task :compile => DefTasks

task :dirs do
  if OPTS['MKBUILDDIRS']
    if !test('d', BINDIR) then mkdir_p BINDIR end
    if !test('d', OBJDIR) then mkdir_p OBJDIR end
  end
end

desc 'remove executables and link them again'
task :relink => [:clean_exec, :default] do
end

desc 'remove compiled *.o files and binaries'
task :clean do
  obj=[];
  Tasks.keys.each do |t|
    obj+=Tasks[t][:obj]
  end
  rm_f DefTasks + obj
end

task :clean_exec do
  rm_f DefTasks
end

desc 'do clean, then compile'
task :rebuild => [:clean, :default]

desc 'execute tests'
task :test => [:compile] do
	Tasks.each_key do |task|
		if Test.has_key?(task)
			print "==>> Making tests for #{task}\n"
			ENV[task]='../../'+Tasks[task][:output]
			Test[task].each do |test|
				print "TEST: #{test[:name]}\n"
				tdir="test/#{task}"
				if test.has_key?(:file)	#execute a file
					tpath="#{tdir}/#{test[:file]}"
					case test[:file].pathmap('%x')
						when '.c' then
							binpath=tpath.ext(BINEXT)
							sh "#{CC[:c][:cc]} #{tpath} -o #{binpath} #{TESTOPTS['.c']} #{test[:flags]}"
							sh "cd #{tdir}; ./#{binpath.pathmap('%f')}"
							print "TEST OK\n"
						when '.sh' then
							sh "cd #{tdir}; ./#{tpath.pathmap('%f')}"
							print "TEST OK\n"
					end
				else	#execute a command
					cmd= test[:command].gsub(/\{\$\w+\}/) {|match|
						if match=~/\{\$#{task}\}/ then r="../../#{Tasks[task][:output]}"
						else r=''
						end
						r
						}
					sh "cd #{tdir}; ./#{cmd}"
					print "TEST OK\n"
				end
			end
		end
	end
end

desc 'install programs'
task :install=>[:default] do
  #install generated files
  Tasks.each_key do |tt|
    installTask Tasks[tt], tt
  end
	#Install docs
	installDocs();
  #install other files as spefified in Rakefile
  Install.each{|f|
    if f.has_key?(:dst)
      installFile f[:src], f[:dst], f[:opts]
    elsif f.has_key?(:type)
      installFile_Type f[:src], f[:type], f[:opts]
    else
      raise "Don't know how to install #{f[:src]}: no destination and no type specified\n";
    end
  }
end

desc 'uninstall programs'
task :uninstall do
end

desc 'uninstall, then install'
task :reinstall=>[:uninstall, :install]

#rule for linking programs
Tasks.each_key do |tt|
  #executable
  if Tasks[tt][:type]==:exec then
    file Tasks[tt][:output] => Tasks[tt][:obj] do |t|
      print "==>> Building #{tt}\n"
      sh "gcc #{t.prerequisites.join(' ')} -o #{t.name} #{LINK_FLAGS} #{Tasks[tt][:link_flags]}"
    end
  #shared library (*.so)
  elsif Tasks[tt][:type]==:shared then
    file Tasks[tt][:output] => Tasks[tt][:obj] do |t|
      print "==>> Building #{tt}\n"
      #~ sh "ar rs lib#{t.name}.a #{t.prerequisites.join(' ')}" #TODO
    end
  #DLL
  elsif Tasks[tt][:type]==:dll then
    file Tasks[tt][:output] => Tasks[tt][:obj] do |t|
      print "==>> Building #{tt}\n"
      #~ sh "dllwrap --def lib#{t.name}.a #{t.prerequisites.join(' ')} -o #{t.name} #{LINK_FLAGS} #{Tasks[tt]['link_flags']}" #TODO
    end
  #static library (*.a)
  elsif Tasks[tt][:type]==:static then
    file Tasks[tt][:output] => Tasks[tt][:obj] do |t|
      print "==>> Building #{tt}\n"
      sh "ar rs #{t.name} #{t.prerequisites.join(' ')}"
    end
  elsif Tasks[tt][:type]==:perl_module then
    file Tasks[tt][:output] do |t|
    end
  else
    raise "Invalid type '#{Tasks[tt][:type]}' for task #{tt}"
  end
end

#rule for compiling sources
#cpp:
rule( /\.o$/ => [ proc {|task_name| task_name.pathmap('%f').ext('.cpp') }]) \
do |t|
  print "==>> Compiling #{t.source}\n"
  sh "#{CC[:cpp][:cc]} #{t.source} -c -o #{t.name} #{$CPP_FLAGS} #{$DEFS} #{SourceFlags[t.source]}";
end
#DEBUG:
rule( /-DEBUG\.o$/ => [ proc {|task_name| task_name.sub('-DEBUG', '').pathmap('%f').ext('.cpp') }]) \
do |t|
  print "==>> Compiling #{t.source}\n"
  sh "#{CC[:cpp][:cc]} #{t.source} -c -o #{t.name} #{$CPP_FLAGS} #{$DEFS} #{SourceFlags[t.source]}";
end

#c:
rule( /\.o$/ => [ proc {|task_name| task_name.pathmap('%f').ext('.c') }]) \
do |t|
  print "==>> Compiling #{t.source}\n"
  sh "#{CC[:c][:cc]} #{t.source} -c -o #{t.name} #{$CPP_FLAGS} #{$DEFS} #{SourceFlags[t.source]}";
end
#DEBUG:
rule( /-DEBUG\.o$/ => [ proc {|task_name| task_name.sub('-DEBUG', '').pathmap('%f').ext('.c') }]) \
do |t|
  print "==>> Compiling #{t.source}\n"
  sh "#{CC[:c][:cc]} #{t.source} -c -o #{t.name} #{$CPP_FLAGS} #{$DEFS} #{SourceFlags[t.source]}";
end

#asm:
rule( /\.o$/ => [ proc {|task_name| task_name.pathmap('%f').ext('.asm') }]) \
do |t|
  print "==>> Compiling #{t.source}\n"
  sh "#{CC[:asm][:cc]} -f #{ASM_OBJ} -o #{t.name} #{$ASM_FLAGS} #{$DEFS} #{SourceFlags[t.source]} #{t.source}";
end
#DEBUG:
rule( /-DEBUG\.o$/ => [ proc {|task_name| task_name.sub('-DEBUG', '').pathmap('%f').ext('.c') }]) \
do |t|
  print "==>> Compiling #{t.source}\n"
  sh "#{CC[:asm][:cc]} -f #{ASM_OBJ} -o #{t.name} #{$ASM_FLAGS} #{$DEFS} #{SourceFlags[t.source]} #{t.source}";
end

