/* interpreter for strack in c - tdwsl 2023 */

#ifndef STRACK_H
#define STRACK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define VARBUF_SZ 120000
#define STACKBUF_SZ 20000
#define STACK_SZ 127
#define DICT_SZ 65536
#define MAX_CFUNS 300
#define MAX_WORDS 600
#define WORDNAMEBUF_SZ 2048

enum {
    INS_DROP=0,
    INS_SWAP, INS_DUP, INS_OVER, INS_NIP, INS_PICK,
    INS_IFDUP, INS_ROT, INS_MROT,
    INS_SETV, INS_GETV, INS_LEFT, INS_RIGHT, INS_APP, INS_LEN,
    INS_SSET, INS_SGET,
    INS_ISNIL, INS_EQU, INS_GRE, INS_LES, INS_GREEQU, INS_LESEQU,
    INS_ADD, INS_SUB, INS_DIV, INS_MOD, INS_MUL,
    INS_AND, INS_OR,
    INS_CHR, INS_ORD,
    INS_EVAL, INS_ISDEFINED, INS_BYE,

    INS_CFUN,
    INS_PUSHSTR,
    INS_CALL,
    INS_RET,
    INS_B,
    INS_BZ,
};

const char *primStrs[] = {
    "DROP",
    "SWAP", "DUP", "OVER", "NIP", "PICK",
    "?DUP", "ROT", "-ROT",
    "!", "@", ":$", "$:", "~", "#",
    "$!", "$@",
    "NOT", "=", ">", "<", ">=", "<=",
    "+", "-", "/", "MOD", "*",
    "AND", "OR",
    "CHR", "ORD",
    "EVAL", "DEF?", "BYE",
    0,
};

const char *cmpStrs[] = {
    "IF", "THEN", "ELSE",
    "BEGIN", "AGAIN", "UNTIL", "WHILE", "REPEAT",
    "EXIT",
    ";",
    0,
};

char varBuf[VARBUF_SZ];
char stackBuf[STACKBUF_SZ];
char *stack[STACK_SZ];
int sp = 0;
uint8_t dict[DICT_SZ];
int here = 0;
int old;
void (*cfuns[MAX_CFUNS])();
const char *cfunNames[MAX_CFUNS];
int ncfuns = 0;
char wordNameBuf[WORDNAMEBUF_SZ];
char *wordNames[MAX_WORDS];
int words[MAX_WORDS];
int nwords = 0;
char cf = 0;
int cstack[128];
int csp = 0;

char *getVar(char *name) {
    char *s;
    s = varBuf;
    while(*s) {
        if(!strcmp(s, name)) return s;
        s += strlen(s)+1;
        s += strlen(s)+1;
    }
    return s;
}

void delVar(char *v) {
    char *e, *p;
    if(!v) return;
    e = v;
    while(*e) {
        e += strlen(e)+1;
        e += strlen(e)+1;
    }
    p = v;
    p += strlen(p)+1;
    p += strlen(p)+1;
    while(p < e)
        *(v++) = *(p++);
    p = v;
    while(p < e)
        *(p++) = 0;
}

void setVar(char *v, char *s) {
    char *e;
    delVar(getVar(v));
    if(!strcmp(s, "NIL")) return;
    e = varBuf;
    while(*e) {
        e += strlen(e)+1;
        e += strlen(e)+1;
    }
    strcpy(e, v);
    e += strlen(e)+1;
    strcpy(e, s);
}

void error(const char *err) {
    printf("error: %s\n", err);
    exit(1);
}

void push(char *s) {
    if(sp) {
        if(sp >= STACK_SZ)
            error("stack overflow");
        stack[sp] = stack[sp-1]+strlen(stack[sp-1])+1;
    } else
        stack[sp] = stackBuf;
    strcpy(stack[sp], s);
    sp++;
}

void assDepth(int d) {
    if(sp < d) error("stack underflow");
}

char *pop() {
    assDepth(1);
    return stack[--sp];
}

void stackDel(int si) {
    int l, i;
    char *s, *p, *e;
    s = stack[si];
    l = strlen(s)+1;
    p = s;
    p += strlen(p)+1;
    e = p;
    while(*e)
        e += strlen(e)+1;
    while(p < e)
        *(s++) = *(p++);
    sp--;
    for(i = si; i <= sp; i++) {
        stack[i] = stack[i+1];
        stack[i] -= l;
    }
}

void append() {
    int l, i;
    char *p;
    assDepth(2);
    p = stack[sp-2]+strlen(stack[sp-2]);
    l = strlen(stack[sp-1]);
    for(i = 0; i <= l; i++)
        (stack[sp-1]-1)[i] = stack[sp-1][i];
    sp--;
}

int number(char *s) {
    int n;
    const char *err = "expected a numeric value";
    char f;

    n = 0;
    f = 0;
    if(*s == '-') { f = 1; s++; }
    if(*s == 0) error(err);
    while(*s) {
        if(*s >= '0' && *s <= '9')
            n = n * 10 + *s - '0';
        else error(err);
        s++;
    }
    if(f) n *= -1;
    return n;
}

void pushNum(int n) {
    char buf[100];
    char rev[100];
    char *p, *s;
    s = rev;
    if(n < 0) { n *= -1; *(p++) = '-'; }
    do {
        *(s++) = (n%10)+'0';
        n /= 10;
    } while(n);
    p = buf;
    while(s != rev)
        *(p++) = *(--s);
    *p = 0;
    push(buf);
}

void pushBool(int b) {
    if(b) push("T");
    else push("NIL");
}

void left() {
    int n, l, i;
    assDepth(2);
    n = number(pop());
    if(n <= 0) { pop(); return; }
    l = strlen(stack[sp-1]);
    l -= n;
    if(l <= 0) return;
    for(i = 0; i <= l; i++)
        stack[sp-1][n+i] = 0;
}

void right() {
    int n, l, i;
    assDepth(2);
    n = number(pop());
    if(n <= 0) { pop(); return; }
    l = strlen(stack[sp-1]);
    if(n >= l) return;
    for(i = 0; i < n; i++)
        stack[sp-1][i] = stack[sp-1][i+l-n];
    for(i = n; i < l; i++)
        stack[sp-1][i] = 0;
}

void inputBuf(char *buf, int max) {
    char c;
    do {
        c = fgetc(stdin);
        *(buf++) = c;
    } while(c != '\n' && --max);
    *(buf-1) = 0;
}

int isDefined(char *s);
void eval(char *s);

void doPrim(int p) {
    int n, m;
    char *s;
    char buf[100];
    /*printf("{%s} ", primStrs[p]);*/
    switch(p) {
    case INS_DROP:
        pop();
        break;
    case INS_SWAP:
        assDepth(2);
        push(stack[sp-2]);
        stackDel(sp-3);
        break;
    case INS_DUP:
        assDepth(1);
        push(stack[sp-1]);
        break;
    case INS_OVER:
        assDepth(2);
        push(stack[sp-2]);
        break;
    case INS_NIP:
        assDepth(2);
        stackDel(sp-2);
        break;
    case INS_PICK:
        n = number(pop());
        assDepth(n+1);
        push(stack[sp-1-n]);
        break;
    case INS_IFDUP:
        assDepth(1);
        if(strcmp(stack[sp-1], "NIL"))
            push(stack[sp-1]);
        break;
    case INS_ROT:
        assDepth(3);
        push(stack[sp-3]);
        stackDel(sp-4);
        break;
    case INS_MROT:
        assDepth(3);
        push(stack[sp-3]);
        stackDel(sp-4);
        push(stack[sp-3]);
        stackDel(sp-4);
        break;
    case INS_SETV:
        assDepth(2);
        setVar(stack[sp-1], stack[sp-2]);
        sp -= 2;
        break;
    case INS_GETV:
        s = getVar(pop());
        if(*s) {
            s += strlen(s)+1;
            push(s);
        } else push("NIL");
        break;
    case INS_LEFT:
        left();
        break;
    case INS_RIGHT:
        right();
        break;
    case INS_APP:
        append();
        break;
    case INS_LEN:
        assDepth(1);
        n = strlen(pop())+1;
        pushNum(n);
        break;
    case INS_SSET:
        assDepth(3);
        n = number(pop());
        if(n >= 0 && n < strlen(stack[sp-2]))
            stack[sp-2][n] = stack[sp-1][0];
        sp--;
        break;
    case INS_SGET:
        assDepth(2);
        n = number(pop());
        if(n >= 0 && n < strlen(stack[sp-1])) {
            buf[0] = stack[sp-1][n];
            buf[1] = 0;
            sp--;
            push(buf);
        } else {
            sp--;
            push("NIL");
        }
        break;
    case INS_ISNIL:
        pushBool(!strcmp(pop(), "NIL"));
        break;
    case INS_EQU:
        pushBool(!strcmp(pop(), pop()));
        break;
    case INS_GRE:
        m = number(pop());
        n = number(pop());
        pushBool(n > m);
        break;
    case INS_LES:
        m = number(pop());
        n = number(pop());
        pushBool(n < m);
        break;
    case INS_GREEQU:
        m = number(pop());
        n = number(pop());
        pushBool(n >= m);
        break;
    case INS_LESEQU:
        m = number(pop());
        n = number(pop());
        pushBool(n <= m);
        break;
    case INS_ADD:
        m = number(pop());
        n = number(pop());
        pushNum(n + m);
        break;
    case INS_SUB:
        m = number(pop());
        n = number(pop());
        pushNum(n - m);
        break;
    case INS_DIV:
        m = number(pop());
        n = number(pop());
        pushNum(n / m);
        break;
    case INS_MOD:
        m = number(pop());
        n = number(pop());
        pushNum(n % m);
        break;
    case INS_MUL:
        m = number(pop());
        n = number(pop());
        pushNum(n * m);
        break;
    case INS_AND:
        pushBool(!!strcmp(pop(), "NIL") & !!strcmp(pop(), "NIL"));
        break;
    case INS_OR:
        pushBool(!!strcmp(pop(), "NIL") | !!strcmp(pop(), "NIL"));
        break;
    case INS_CHR:
        n = number(pop());
        buf[0] = n;
        buf[1] = 0;
        push(buf);
        break;
    case INS_ORD:
        pushNum(*pop());
        break;
    case INS_EVAL:
        eval(pop());
        break;
    case INS_ISDEFINED:
        pushBool(isDefined(pop()));
        break;
    case INS_BYE:
        exit(0);
        break;
    }
}

int strIndex(const char **arr, char *s) {
    int i;
    for(i = 0; arr[i]; i++)
        if(!strcmp(arr[i], s)) return i;
    return -1;
}

int strnIndex(const char **arr, int n, char *s) {
    int i;
    for(i = 0; i < n; i++)
        if(!strcmp(arr[i], s)) return i;
    return -1;
}

void addIns(int ins) {
    dict[here++] = ins;
}

void addStr(char *s) {
    addIns(INS_PUSHSTR);
    while(*s) addIns(*(s++));
    addIns(0);
}

int getD(int i) {
    int n;
    n = dict[i] + dict[i+1]*256;
    if(n & 0x8000)
        n = ((~n+1)&0xffff)*-1;
    return n;
}

void setD(int i, int n) {
    dict[i] = (unsigned)n;
    dict[i+1] = (unsigned)n/256;
}

void addD(int n) {
    setD(here, n);
    here += 2;
}

void run(int pc) {
    char buf[200];
    char *p;
    for(;;) {
        if(dict[pc] <= INS_BYE) { doPrim(dict[pc++]); continue; }
        switch(dict[pc++]) {
        case INS_CALL:
            pc += 2;
            run(words[getD(pc-2)]);
            break;
        case INS_RET:
            return;
        case INS_PUSHSTR:
            p = buf;
            while(dict[pc]) *(p++) = dict[pc++];
            pc++;
            *p = 0;
            push(buf);
            break;
        case INS_CFUN:
            pc += 2;
            cfuns[getD(pc-2)]();
            break;
        case INS_BZ:
            pc += 2;
            if(!strcmp(pop(), "NIL"))
                pc += getD(pc-2);
            break;
        case INS_B:
            pc += 2;
            pc += getD(pc-2);
            break;
        }
    }
}

void addWord(char *name) {
    char *p;
    if(nwords) p = wordNames[nwords-1]+strlen(wordNames[nwords-1])+1;
    else p = wordNameBuf;
    strcpy(p, name);
    wordNames[nwords] = p;
    words[nwords] = here;
    nwords++;
}

int findWord(char *s) {
    int i;
    for(i = nwords-1; i >= 0; i--)
        if(!strcmp(wordNames[i], s)) return i;
    return -1;
}

int isDefined(char *s) {
    if(strIndex(primStrs, s) != -1) return 1;
    if(strnIndex(cfunNames, ncfuns, s) != -1) return 1;
    if(findWord(s) != -1) return 1;
    return 0;
}

void getStr(char *buf, int i) {
    while(dict[i]) *(buf++) = dict[i++];
    *buf = 0;
}

void printString(char *s, const char *e) {
    printf("\"");
    for(; *s; s++)
        switch(*s) {
        case '"': printf("\\\""); break;
        case '\\': printf("\\\\"); break;
        case '\n': printf("\\n"); break;
        case '\t': printf("\\t"); break;
        default: printf("%c", *s); break;
        }
    printf("\"%s", e);
}

void printWord(int n) {
    int i, m;
    char buf[200];
    printf("%s\n", wordNames[n]);
    m = (n >= nwords-1) ? here : words[n+1];
    for(i = words[n]; i < m; i++) {
        printf("%.8X ", i);
        if(dict[i] <= INS_BYE) {
            printf("%s\n", primStrs[dict[i]]);
            continue;
        }
        switch(dict[i]) {
        case INS_CALL:
            printf("CALL %s\n", wordNames[getD(i+1)]);
            i += 2;
            break;
        case INS_CFUN:
            printf("CFUN %s\n", cfunNames[getD(i+1)]);
            i += 2;
            break;
        case INS_RET:
            printf("RET\n");
            break;
        case INS_B:
            printf("B %d\n", getD(i+1));
            i += 2;
            break;
        case INS_BZ:
            printf("BZ %d\n", getD(i+1));
            i += 2;
            break;
        case INS_PUSHSTR:
            getStr(buf, i+1);
            printf("PUSHSTR ");
            printString(buf, "\n");
            i += strlen(buf)+1;
            break;
        }
    }
}

void runLine(char *s) {
    char buf[200];
    char *p;
    int n;
    while(*s) {
        while(*s && *s <= ' ') s++;
        p = buf;
        if(*s == '"') {
            while(*(++s) != '"') {
                if(*s == '\\') {
                    switch(*(++s)) {
                    case '\\':
                        *(p++) = '\\';
                        break;
                    case 'n':
                        *(p++) = '\n';
                        break;
                    case 't':
                        *(p++) = '\t';
                        break;
                    case '"':
                        *(p++) = '"';
                        break;
                    default:
                        error("unknown escape character");
                    }
                } else if(*s == 0)
                    error("unterminated quote");
                else
                    *(p++) = *s;
            }
            s++;
            *p = 0;
            if(cf & 2) continue;
            if(cf) addStr(buf);
            else push(buf);
        } else {
            while(*s > ' ') {
                *p = *(s++);
                if(*p >= 'a' && *p <= 'z') *p += 'A'-'a';
                p++;
            }
            *p = 0;
            if(cf & 2) {
                if(!strcmp(buf, ")")) cf ^= 2;
                continue;
            }
            if(!strcmp(buf, "\\")) return;
            if(!strcmp(buf, "(")) { cf |= 2; continue; }
        redo:
            if(cf) {
                if((n = strIndex(primStrs, buf)) != -1) {
                    addIns(n);
                } else if((n = findWord(buf)) != -1) {
                    addIns(INS_CALL);
                    setD(here, n);
                    here += 2;
                } else if((n = strnIndex(cfunNames, ncfuns, buf)) != -1) {
                    addIns(INS_CFUN);
                    setD(here, n);
                    here += 2;
                } else if(!strcmp(":", buf)) {
                    error("used : in word");
                } else if(!strcmp(";", buf)) {
                    if(csp) error("compile stack not zero");
                    addIns(INS_RET);
                    cf = 0;
                } else if(!strcmp("IF", buf)) {
                    addIns(INS_BZ);
                    cstack[csp++] = here;
                    here += 2;
                } else if(!strcmp("THEN", buf)) {
                    setD(cstack[csp-1], here-cstack[csp-1]-2);
                    csp--;
                } else if(!strcmp("ELSE", buf)) {
                    addIns(INS_B);
                    setD(cstack[csp-1], here+2-cstack[csp-1]-2);
                    cstack[csp-1] = here;
                    here += 2;
                } else if(!strcmp("BEGIN", buf)) {
                    cstack[csp++] = here;
                } else if(!strcmp("UNTIL", buf)) {
                    addIns(INS_BZ);
                    addD(cstack[--csp]-2-here);
                } else if(!strcmp("AGAIN", buf)) {
                    addIns(INS_B);
                    addD(cstack[--csp]-2-here);
                } else if(!strcmp("WHILE", buf)) {
                    addIns(INS_BZ);
                    cstack[csp++] = here;
                    here += 2;
                } else if(!strcmp("REPEAT", buf)) {
                    addIns(INS_B);
                    addD(cstack[csp-2]-2-here);
                    setD(cstack[csp-1], here-cstack[csp-1]-2);
                    csp -= 2;
                } else if(!strcmp("EXIT", buf)) {
                    addIns(INS_RET);
                } else {
                    addStr(buf);
                }
                if(csp == 0 && (cf & 4)) {
                    addIns(INS_RET);
                    run(old);
                    here = old;
                    cf = 0;
                }
            } else if(!strcmp(":", buf)) {
                addWord(pop());
                cf = 1;
            } else if((n = strIndex(primStrs, buf)) != -1) {
                doPrim(n);
            } else if((n = strIndex(cmpStrs, buf)) != -1) {
                /*printf("%s is compile only\n", buf);
                error("compile only word");*/
                old = here;
                cf = 5;
                goto redo;
            } else if((n = strnIndex(cfunNames, ncfuns, buf)) != -1) {
                cfuns[n]();
            } else if((n = findWord(buf)) != -1) {
                run(words[n]);
            } else {
                push(buf);
            }
        }
    }
    if(cf & 2) error("unterminated comment");
    fflush(stdout);
}

void eval(char *s) {
    char *p;
    char buf[2048];
    strcpy(buf, s);
    s = buf;
    while(s) {
        p = s;
        s = strchr(s, '\n');
        if(s) *(s++) = 0;
        runLine(p);
    }
    if(cf&1) error("unterminated word in eval");
    cf = 0;
}

void runFile(const char *filename) {
    FILE *fp;
    char c;
    char buf[500];
    char *p;
    fp = fopen(filename, "r");
    if(!fp) {
        printf("failed to open %s\n", filename);
        error("failed to open file");
    }
    while(!feof(fp)) {
        p = buf;
        do {
            c = fgetc(fp);
            *(p++) = c;
        } while(!feof(fp) && c && c != '\n');
        *(p-1) = 0;
        runLine(buf);
    }
    fclose(fp);
}

void addCfun(const char *name, void (*fun)()) {
    cfuns[ncfuns] = fun;
    cfunNames[ncfuns] = name;
    ncfuns++;
}

void f_print() {
    printf("%s", pop());
}

void f_input() {
    char buf[100];
    inputBuf(buf, 100);
    push(buf);
}

void f_printStack() {
    int i;
    printf("stack(%d): ", sp);
    for(i = 0; i < sp; i++)
        printString(stack[i], " ");
    printf("\n");
}

void f_see() {
    int n;
    n = findWord(pop());
    if(n == -1)
        error("word not found");
    printWord(n);
}

void f_printVars() {
    int i;
    char *s;
    s = varBuf;
    while(*s) {
        printString(s, " = ");
        s += strlen(s)+1;
        printString(s, "\n");
        s += strlen(s)+1;
    }
}

void init() {
    int i;
    /*bzero(varBuf, VARBUF_SZ);
    bzero(stackBuf, STACKBUF_SZ);*/
    for(i = 0; i < VARBUF_SZ; i++) varBuf[i] = 0;
    for(i = 0; i < STACKBUF_SZ; i++) stackBuf[i] = 0;
    addCfun("INPUT", f_input);
    addCfun(".", f_print);
    addCfun(".S", f_printStack);
    addCfun("SEE", f_see);
    addCfun(".V", f_printVars);
}

#endif
