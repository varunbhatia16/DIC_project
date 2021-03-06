# Generated automatically from Makefile.in by configure.
ALL: default
##### User configurable options #####

MPIR_HOME    = 
CC           = ../../../../bin/ampiCC
CLINKER      = ../../../../bin/ampiCC
CCC          = 
CCLINKER     = $(CCC)
F77          = :
F90BASE	     = @MPIF90BASE@
F90          = @MPIF90@
FLINKER      = :
OPTFLAGS     = 
MPIFDEP      = 
### End User configurable options ###

SHELL = /bin/sh
prefix     = 
top_srcdir = ..
srcdir     = ../pt2pt
VPATH=.:$(srcdir)

PROFLIB =
CFLAGS  =   -DHAVE_STDLIB_H=1 -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DUSE_STDARG=1 -DHAVE_LONG_DOUBLE=1 -DHAVE_LONG_LONG_INT=1 -DHAVE_PROTOTYPES=1 -DHAVE_SIGNAL_H=1 -DHAVE_SIGACTION=1 -DHAVE_SLEEP=1 -DHAVE_SYSCONF=1 $(OPTFLAGS) 
CCFLAGS = $(CFLAGS)
FFLAGS = $(OPTFLAGS) 
# Use LIBS to add any special libraries for C programs
LIBS =  
# Use FLIBS to add any special libraries for Fortran programs
FLIBS =  
EXECS = overtake sendrecv isndrcv third fourth fifth sixth hvectest \
	hvectest2 hvec probe probe1 testtest1 ssendtest ssendtest2 typetest\
	dtypelife issendtest issend2 issendinit relrank nullproc trunc \
	truncmult typeub typeub2 typeub3 irecvtest htmsg nullproc2 \
	isendtest self sendrecv2 sendrecv3 sendrecv4 persistent waitany \
	pack bsendtest typebase irsend irsendinit longmsgs waitall waitall2 \
	testsome getelm testall sndrcv nbtest nblock sendmany order hindexed \
	commit cancel persist typecreate reqcreate sndrcvrep flood flood2 \
	persist2 dataalign dtyperecv reqfree sndrcvrpl2 cancel2 cancel3 \
	cancelmessages cancelibm cancelissend \
	sendorder exittest selfvsworld waitall3 \
	waitall4 typelb structlb
OTHEREXECS = testtypes secondf allpair systest send1 \
		systest1 allpair2 bsendtest structf typebasef sendfort bcastf \
		isendf
default: $(EXECS)

#
# Note that runtests builds the executables as required
testing: 
	  -./runtests $(TESTARGS)

# We want the link tests to be noisier about problems
linktest: 
	-$(MAKE) overtake
	@if [ ! -x overtake ] ; then \
	    echo "Could not link a C program with MPI libraries" ; \
	    exit 1 ; fi
	$(MAKE) clean
#
# This now becomes part of the Fortran module
#	-$(MAKE) secondf
#	@if [ ! -x secondf ] ; then \
#	    echo "Could not link a Fortran program with MPI libraries" ; \
#	    exit 1 ; fi

linktest1: 
	-$(MAKE) overtake
	@if [ ! -x overtake ] ; then \
	    echo "Could not link a C program with MPI libraries" ; \
	    exit 1 ; fi
	$(MAKE) clean

all: testing

fortran_tests: secondf allpair allpair2 structf typebasef send1 pingpong \
	sendfort

#f90_tests: secondf90 allpair90 allpair290 structf90 typebasef90 send190 \
#	pingpong90

overtake: overtake.o test.o
	$(CLINKER) $(OPTFLAGS) -o overtake overtake.o test.o $(LIBS)

bsendtest: bsendtest.o test.o
	$(CLINKER) $(OPTFLAGS) -o bsendtest bsendtest.o test.o $(LIBS)

relrank: relrank.o test.o
	$(CLINKER) $(OPTFLAGS) -o relrank relrank.o test.o $(LIBS)

sendrecv: sendrecv.o test.o
	$(CLINKER) $(OPTFLAGS) -o sendrecv sendrecv.o test.o $(LIBS)

persistent: persistent.o
	$(CLINKER) $(OPTFLAGS) -o persistent persistent.o $(LIBS)

flood: flood.o test.o
	$(CLINKER) $(OPTFLAGS) -o flood flood.o test.o $(LIBS)

flood2: flood2.o test.o
	$(CLINKER) $(OPTFLAGS) -o flood2 flood2.o test.o $(LIBS)

persist: persist.o
	$(CLINKER) $(OPTFLAGS) -o persist persist.o $(LIBS)

persist2: persist2.o
	$(CLINKER) $(OPTFLAGS) -o persist2 persist2.o $(LIBS)

sendrecv2: sendrecv2.o dtypes.o gcomm.o
	$(CLINKER) $(OPTFLAGS) -o sendrecv2 sendrecv2.o dtypes.o gcomm.o $(LIBS)

sendrecv3: sendrecv3.o dtypes.o gcomm.o
	$(CLINKER) $(OPTFLAGS) -o sendrecv3 sendrecv3.o dtypes.o gcomm.o $(LIBS)

sendrecv4: sendrecv4.o dtypes.o gcomm.o
	$(CLINKER) $(OPTFLAGS) -o sendrecv4 sendrecv4.o dtypes.o gcomm.o $(LIBS)

irsend: irsend.o dtypes.o gcomm.o
	$(CLINKER) $(OPTFLAGS) -o irsend irsend.o dtypes.o gcomm.o $(LIBS)

irsendinit: irsendinit.o dtypes.o gcomm.o
	$(CLINKER) $(OPTFLAGS) -o irsendinit irsendinit.o dtypes.o gcomm.o $(LIBS)

longmsgs: longmsgs.o
	$(CLINKER) $(OPTFLAGS) -o longmsgs longmsgs.o $(LIBS)

testsome: testsome.o dtypes.o gcomm.o
	$(CLINKER) $(OPTFLAGS) -o testsome testsome.o dtypes.o gcomm.o $(LIBS)

testall: testall.o dtypes.o gcomm.o
	$(CLINKER) $(OPTFLAGS) -o testall testall.o dtypes.o gcomm.o $(LIBS)

sndrcv: sndrcv.o dtypes.o gcomm.o
	$(CLINKER) $(OPTFLAGS) -o sndrcv sndrcv.o dtypes.o gcomm.o $(LIBS)

sndrcvrep: sndrcvrep.o
	$(CLINKER) $(OPTFLAGS) -o sndrcvrep sndrcvrep.o $(LIBS)

sndrcvrpl2: sndrcvrpl2.o
	$(CLINKER) $(OPTFLAGS) -o sndrcvrpl2 sndrcvrpl2.o $(LIBS)

getelm: getelm.o 
	$(CLINKER) $(OPTFLAGS) -o getelm getelm.o $(LIBS)

self: self.o test.o
	$(CLINKER) $(OPTFLAGS) -o self self.o test.o $(LIBS)

typeub: typeub.o test.o
	$(CLINKER) $(OPTFLAGS) -o typeub typeub.o $(LIBS)

typeub2: typeub2.o
	$(CLINKER) $(OPTFLAGS) -o typeub2 typeub2.o $(LIBS)

typeub3: typeub3.o
	$(CLINKER) $(OPTFLAGS) -o typeub3 typeub3.o $(LIBS)

typecreate: typecreate.o
	$(CLINKER) $(OPTFLAGS) -o typecreate typecreate.o $(LIBS)

reqcreate: reqcreate.o
	$(CLINKER) $(OPTFLAGS) -o reqcreate reqcreate.o $(LIBS)

reqfree: reqfree.o
	$(CLINKER) $(OPTFLAGS) -o reqfree reqfree.o $(LIBS)

isndrcv: isndrcv.o test.o
	$(CLINKER) $(OPTFLAGS) -o isndrcv isndrcv.o test.o $(LIBS)

nullproc: nullproc.o
	$(CLINKER) $(OPTFLAGS) -o nullproc nullproc.o $(LIBS)

nullproc2: nullproc2.o
	$(CLINKER) $(OPTFLAGS) -o nullproc2 nullproc2.o $(LIBS)

trunc: trunc.o
	$(CLINKER) $(OPTFLAGS) -o trunc trunc.o $(LIBS)

truncmult: truncmult.o
	$(CLINKER) $(OPTFLAGS) -o truncmult truncmult.o $(LIBS)

typetest: typetest.o test.o
	$(CLINKER) $(OPTFLAGS) -o typetest typetest.o test.o $(LIBS)

typebase: typebase.o
	$(CLINKER) $(OPTFLAGS) -o typebase typebase.o $(LIBS)

ssendtest: ssendtest.o test.o
	$(CLINKER) $(OPTFLAGS) -o ssendtest ssendtest.o test.o $(LIBS)

ssendtest2: ssendtest2.o
	$(CLINKER) $(OPTFLAGS) -o ssendtest2 ssendtest2.o $(LIBS)

issendtest: issendtest.o test.o
	$(CLINKER) $(OPTFLAGS) -o issendtest issendtest.o test.o $(LIBS)

issend2: issend2.o
	$(CLINKER) $(OPTFLAGS) -o issend2 issend2.o $(LIBS)

issendinit: issendinit.o test.o
	$(CLINKER) $(OPTFLAGS) -o issendinit issendinit.o test.o $(LIBS)

testtypes: testtypes.o test.o
	$(CLINKER) $(OPTFLAGS) -o testtypes testtypes.o test.o $(LIBS)

dataalign: dataalign.o
	$(CLINKER) $(OPTFLAGS) -o dataalign dataalign.o $(LIBS)

dtyperecv: dtyperecv.o
	$(CLINKER) $(OPTFLAGS) -o dtyperecv dtyperecv.o $(LIBS)

dtypelife: dtypelife.o test.o
	$(CLINKER) $(OPTFLAGS) -o dtypelife dtypelife.o test.o $(LIBS)

htmsg: htmsg.o
	$(CLINKER) $(OPTFLAGS) -o htmsg htmsg.o $(LIBS)

isendtest: isendtest.o
	$(CLINKER) $(OPTFLAGS) -o isendtest isendtest.o $(LIBS)

third: third.o test.o
	$(CLINKER) $(OPTFLAGS) -o third third.o test.o $(LIBS)

fourth: fourth.o test.o
	$(CLINKER) $(OPTFLAGS) -o fourth fourth.o test.o $(LIBS)

fifth: fifth.o test.o
	$(CLINKER) $(OPTFLAGS) -o fifth fifth.o test.o $(LIBS)

sixth: sixth.o test.o
	$(CLINKER) $(OPTFLAGS) -o sixth sixth.o test.o $(LIBS)

probe: probe.o test.o
	$(CLINKER) $(OPTFLAGS) -o probe probe.o test.o $(LIBS)

nblock: nblock.o 
	$(CLINKER) $(OPTFLAGS) -o nblock nblock.o $(LIBS)

sendmany: sendmany.o 
	$(CLINKER) $(OPTFLAGS) -o sendmany sendmany.o $(LIBS)

order: order.o
	$(CLINKER) $(OPTFLAGS) -o order order.o $(LIBS)

pack: pack.o
	$(CLINKER) $(OPTFLAGS) -o pack pack.o $(LIBS)

probe1: probe1.o test.o
	$(CLINKER) $(OPTFLAGS) -o probe1 probe1.o test.o $(LIBS)

testtest1: testtest1.o test.o 
	$(CLINKER) $(OPTFLAGS) -o testtest1 testtest1.o test.o $(LIBS)

hvectest: hvectest.o test.o
	$(CLINKER) $(OPTFLAGS) -o hvectest hvectest.o test.o $(LIBS)

hvectest2: hvectest2.o test.o
	$(CLINKER) $(OPTFLAGS) -o hvectest2 hvectest2.o test.o $(LIBS)

hvec: hvec.o
	$(CLINKER) $(OPTFLAGS) -o hvec hvec.o $(LIBS)

hindexed: hindexed.o 
	$(CLINKER) $(OPTFLAGS) -o hindexed hindexed.o $(LIBS)

irecvtest: irecvtest.o test.o
	$(CLINKER) $(OPTFLAGS) -o irecvtest irecvtest.o test.o $(LIBS)

nbtest: nbtest.o
	$(CLINKER) $(OPTFLAGS) -o nbtest nbtest.o $(LIBS)

waitany: waitany.o
	$(CLINKER) $(OPTFLAGS) -o waitany waitany.o $(LIBS)

waitall: waitall.o
	$(CLINKER) $(OPTFLAGS) -o waitall waitall.o $(LIBS)

waitall2: waitall2.o
	$(CLINKER) $(OPTFLAGS) -o waitall2 waitall2.o $(LIBS)

waitall3: waitall3.o
	$(CLINKER) $(OPTFLAGS) -o waitall3 waitall3.o $(LIBS)

waitall4: waitall4.o
	$(CLINKER) $(OPTFLAGS) -o waitall4 waitall4.o $(LIBS)

commit: commit.o
	$(CLINKER) $(OPTFLAGS) -o commit commit.o $(LIBS)

cancel: cancel.o
	$(CLINKER) $(OPTFLAGS) -o cancel cancel.o $(LIBS)

cancel2: cancel2.o
	$(CLINKER) $(OPTFLAGS) -o cancel2 cancel2.o $(LIBS)

cancel3: cancel3.o
	$(CLINKER) $(OPTFLAGS) -o cancel3 cancel3.o $(LIBS)

cancelmessages: cancelmessages.o
	$(CLINKER) $(OPTFLAGS) -o cancelmessages cancelmessages.o $(LIBS)

cancelibm: cancelibm.o
	$(CLINKER) $(OPTFLAGS) -o cancelibm cancelibm.o $(LIBS)

cancelissend: cancelissend.o
	$(CLINKER) $(OPTFLAGS) -o cancelissend cancelissend.o $(LIBS)

sendorder: sendorder.o
	$(CLINKER) $(OPTFLAGS) -o sendorder sendorder.o $(LIBS)

exittest: exittest.o
	$(CLINKER) $(OPTFLAGS) -o exittest exittest.o $(LIBS)

selfvsworld: selfvsworld.o
	$(CLINKER) $(OPTFLAGS) -o selfvsworld selfvsworld.o $(LIBS)

# This has a test for HAS_FORTRAN so that the link test target will work
# even when -nof77 is selected.
secondf: $(MPIFDEP) secondf.o
	if test "0" = 1 ; then \
	$(FLINKER) $(OPTFLAGS) -o ./secondf secondf.o $(FLIBS) ; fi

allpair: $(MPIFDEP) allpair.o 
	$(FLINKER) $(OPTFLAGS) -o ./allpair allpair.o $(FLIBS)

allpair2: $(MPIFDEP) allpair2.o
	$(FLINKER) $(OPTFLAGS) -o ./allpair2 allpair2.o $(FLIBS)

structf: $(MPIFDEP) structf.o
	$(FLINKER) $(OPTFLAGS) -o ./structf structf.o $(FLIBS)

send1: $(MPIFDEP) send1.o
	$(FLINKER) $(OPTFLAGS) -o ./send1 send1.o $(FLIBS)

isendf: $(MPIFDEP) isendf.o
	$(FLINKER) $(OPTFLAGS) -o ./isendf isendf.o $(FLIBS)

sendfort: $(MPIFDEP) sendfort.o
	$(FLINKER) $(OPTFLAGS) -o ./sendfort sendfort.o $(FLIBS)

pingpong: $(MPIFDEP) pingpong.o
	$(FLINKER) $(OPTFLAGS) -o ./pingpong pingpong.o $(FLIBS)

typebasef: $(MPIFDEP) typebasef.o
	$(FLINKER) $(OPTFLAGS) -o ./typebasef typebasef.o $(FLIBS)

#
# Make sure that we remove executables for specific architectures
clean:
	@-rm -f *.o *~ PI* $(EXECS) *.out core pt2pt.diff $(OTHEREXECS) \
		*.trace rdb.* startup.* mpif.h ${srcdir}/*.o *.stdo
	@-rm -f work.pc work.pcl
	@-for file in $(EXECS) Makefile ; do \
	   rm -f $$file.sun4 $$file.alpha $$file.IRIX $$file.freebsd ;\
	done
.c:
	$(CC) $(CFLAGS) -o $* $< $(LIBS)
.c.o:
	$(CC) $(CFLAGS) -c $<
.o:
	${CLINKER} $(OPTFLAGS) -o $* $*.o $(LIBS)
.f.o:
	$(F77) $(FFLAGS) -c $<
