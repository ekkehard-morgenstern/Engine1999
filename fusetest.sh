#!/bin/sh
# Differential test of the VM's fused instructions (vm.c "fused instructions"): every program runs with fusing (the default) and
# with E1999_NOFUSE=1, in the console host; the output (results and error messages) must be identical. Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
T=$(mktemp -d) || exit 1
trap 'rm -f "$T"/*.bas "$T"/*.a "$T"/*.b; rmdir "$T"' EXIT
fail=0
n=0
t() {   # name; the program is on standard input
    n=$((n+1))
    cat > "$T/p$n.bas"
    timeout 60 ./engine1999c "$T/p$n.bas" > "$T/p$n.a" 2>&1
    E1999_NOFUSE=1 timeout 60 ./engine1999c "$T/p$n.bas" > "$T/p$n.b" 2>&1
    if cmp -s "$T/p$n.a" "$T/p$n.b" && [ -s "$T/p$n.a" ]; then printf 'PASS  %s\n' "$1"; else printf 'FAIL  %s\n' "$1"; diff "$T/p$n.a" "$T/p$n.b" | head -5; fail=$((fail+1)); fi
}
t "loops, arithmetic, 1D / 2D arrays" <<'BAS'
10 DIM A(60),B%(60),M(8,9),N%(8,9)
20 FOR I=1 TO 49:A(I)=A(I-1)*0.5+I:B%(I)=B%(I-1)+3:NEXT
30 FOR I=6 TO 1 STEP -1:FOR J=1 TO 6:M(I,J)=M(I-1,J)+M(I,J-1)+I*J:N%(I,J)=N%(I-1,J)-J:NEXT J,I
40 FOR I=10 DOWNTO 1:S=S+I*I-A(I)/3+B%(I):NEXT
50 FOR X=0.5 TO 3.2 STEP 0.7:T=T+X:NEXT
60 PRINT S,T,A(49),B%(49),M(6,6),N%(6,6)
70 FOR I=5 TO 1:PRINT "never":NEXT
80 FOR I=1 TO 3:FOR J=3 DOWNTO 1:K=K+I*10+J:NEXT:NEXT:PRINT K
90 I=5:WHILE I>0:I=I-1:IF I AND 1 THEN C=C+1
100 WEND:PRINT C,I;1 OR 4;7 XOR 2;(3<4);(3>4);(3=3);(3<>3);(3<=2);(3>=3)
BAS
t "errors in fused paths are the ordinary ones" <<'BAS'
10 DIM A(5),B%(5),M(3,3)
20 ON ERROR GOTO 100
30 I=7:PRINT A(I)
40 I=-1:PRINT A(I)
50 B%(2)=40000
60 X=1:Y=0:PRINT X/Y
70 PRINT M(2,5):M(4,1)=1:J=2:PRINT A(1,J)
80 PRINT 1/(Y-Y):PRINT "done"
90 END
100 PRINT "err";ERR;ERR$;" in";ERL:RESUME NEXT
BAS
t "subscript truncation, big numbers, negative zero" <<'BAS'
10 DIM A(8),B%(8)
20 I=0.9:A(I)=1:I=-0.5:A(I)=A(I)+2:B%(1)=32767:B%(2)=-32768
30 PRINT A(0),A(1),B%(1)+1,B%(2)-1,2^31+2^31
40 I=2.5:A(I)=B%(I):PRINT A(2)
50 X=10^300:PRINT X*X>10^308
60 B%(3)=A(0)*30000+40000
BAS
t "strings mixed with numbers" <<'BAS'
10 DIM S$(4):A$="x":FOR I=1 TO 3:S$(I)=A$+STR$(I):A$=A$+"y":NEXT:PRINT S$(1);S$(2);S$(3)
20 ON ERROR GOTO 50
30 X=1:Y=VAL(A$)+X:PRINT Y:Z=LEN(A$)*X:PRINT Z:W$=A$+STR$(1)
40 PRINT "end":END
50 PRINT ERR$:RESUME NEXT
BAS
t "GOSUB, procedures, jumps into loops, TRON" <<'BAS'
10 DEF FN SQ(X)=X*X
20 FOR I=1 TO 4:GOSUB 100:NEXT:PRINT T
30 CALL P(3):TRACE ON:FOR I=1 TO 2:PRINT I:NEXT:TRACE OFF
40 END
100 T=T+FN SQ(I)
110 RETURN
200 PROC P(N)
210 FOR K=1 TO N:PRINT K*2;:NEXT:PRINT
220 END PROC
BAS
t "timers and handlers" <<'BAS'
10 DIM A(20)
20 EVERY 1 GOSUB 100
30 FOR I=1 TO 20000:A(I MOD 20)=A(I MOD 20)+1:NEXT
40 REM
50 REM the number of ticks varies: only print that the loop was complete
60 S=0:FOR I=0 TO 19:S=S+A(I):NEXT:PRINT S
70 END
100 RETURN
BAS
t "data constants and READ" <<'BAS'
10 DATA 1.5,2.25,3
20 READ A,B,C:X=A*0.1+B*0.25-0.125:PRINT A,B,C,X,X*1.5E3+0.0625
BAS
[ -n "$FUSE_SHOW" ] && for f in "$T"/*.a; do echo "== $f"; cat "$f"; done
printf '%s\n' "fusetest: $((n-fail)) of $n passed"
exit $fail
