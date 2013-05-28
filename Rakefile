#Rakefile for bfk v0.30
import 'Rakefile.inc';

CPP_FLAGS=['-std=gnu99'];
ASM_FLAGS=[];
OPTS={
	'MKBUILDDIRS'=>true,	# whether to create bin and obj directories
}

Tasks={
  'bfk'=>{
    :type=>:exec,
    :sources=>%w[bfk.c bfk_ex.c],
    :link_flags=>['-pthread'],
  }
}

SourceFlags={
	'bfk.c'=>['-Os -D_POSIX_C_SOURCE=200112L'],	#_POSIX_C_SOURCE needed by linux for posix_fadvice
	'bfk_ex.c'=>['-O2'],
}

Test={
	'bfk'=>[
		{:name=>'basics', :file=>'test-bf.sh'}
	]
}

Install=[
  #{:src=>'data/pMenu.server', :dst=>'/usr/lib/bonobo/servers', :opts=>{}},
  #{:src=>'Sterling', :type=>:ruby_lib, :opts=>{}},
]
