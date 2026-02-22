/**
 * Project: Pixie
 *
 * File: ppext.h
 *
 * Description:
 *   This file defines the interface for ppext.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

/************************************************************************/
/*									*/
/*	File:	ppext.h							*/
/*									*/
/*	External function prototype definitions for PP.			*/
/*									*/
/************************************************************************/

/*
 *	pp1.c
 */
extern int preprocess(char *inFile, FILE *outFile, int argc, char **argv);
extern char *getnext(char *cp, int *argc, char ***argv, int swvalid);
extern void init(void);
extern void usage(int v);

/*
 *	pp2.c
 */
extern char *docall(struct symtab *p, char *internal, char *internal_limit);
extern char *_docall(char *line, char *internal, char *internal_limit);
extern void dodefine(int mactype, int no_flag, const char *name);
extern void doerror(int dummy, int no_flag, const char *name);
extern void doundef(int dummy, int no_flag, const char *name);
extern char *esc_str(char *old, int c, char *limit);
extern void fbind(struct symtab **formals, const char *name, const char *value);
extern char *flookup(struct symtab *formals, const char *name);
extern struct param *getparams(void);
extern unsigned int hash(const char *sym);
extern struct symtab *lookup(const char *name, struct symtab **pe);
extern struct param *makeparam(const char *s, int f);
extern struct ppdir *predef(const char *n, struct ppdir *table);
extern void sbind(const char *sym, const char *defn, struct param *params);
extern char *strize(char *result, char *limit, const char *msg, const char *new_str);
extern void unfbind(struct symtab *formals);
extern void unparam(struct param *pp);
extern void unsbind(const char *sym);

/*
 *	pp3.c
 */
extern void cur_user(void);
extern void do_line(char at_bol);
extern void doinclude(int dummy, int no_flag, const char *name);
extern void doline(int dummy, int no_flag, const char *name);
extern int gchbuf(void);
extern int gchfile(void);
extern int gchpb(void);
extern int getchn(void);
#if HOST == H_CPM
extern int inc_open(const char *incfile, int u, int d);
#else
extern int inc_open(const char *incfile);
#endif
extern void init_path(void);
extern int popfile(void);
extern char *readline(char *buf, int bufsize, int flags, int doexpand);
extern void scaneol(void);
extern void set_user(void);
extern int trigraph(void);

/*
 *	pp4.c
 */
extern char *addstr(char *old, char *limit, const char *msg, const char *new_str);
extern int getnstoken(int f);
extern int gettoken(int f);
#if !PP
extern int istype(int c, int v);
#endif /* !PP */
extern void memmov(const char *src, char *dest, unsigned len);
extern void pbcstr(char *s);
extern void pbstr(const char *in);
extern void pushback(int c);
extern void puttoken(const char s[]);
extern int type(int c);

/*
 *	pp5.c
 */
extern void doelse(int elif, int no_flag, const char *name);
extern void doendif(int dummy, int no_flag, const char *name);
extern void doif(int dummy, int no_flag, const char *name);
extern void doifs(int t, int no_flag, const char *name);

/*
 *	pp6.c
 */
#if (TARGET == T_QC) OR(TARGET == T_QCX) OR(TARGET == T_TCX)
extern void doasm(int asmtype, int no_flag, const char *name);
#endif /* (TARGET == T_QC) OR (TARGET == T_QCX) OR (TARGET == T_TCX) */

extern void dopragma(int dummy, int no_flag, const char *name);

#if (TARGET == T_QC) OR(TARGET == T_QCX) OR(TARGET == T_TCX)
extern void pragasm(int asmtype, int no_flag, const char *name);
#endif /* (TARGET == T_QC) OR (TARGET == T_QCX) OR (TARGET == T_TCX) */

extern void pragendm(int dummy, int no_flag, const char *name);
extern void pragerror(int dummy, int no_flag, const char *name);
extern void pragmsg(int dummy, int no_flag, const char *name);
extern void pragopt(int dummy, int no_flag, const char *name);
extern void pragvalue(int dummy, int no_flag, const char *name);

/*
 *	pp7.c
 */
extern void end_of_file(void);
extern void fatal(const char *s1, const char *s2);
extern void illegal_symbol(void);
extern void non_fatal(const char *s1, const char *s2);
extern void out_of_memory(void);
extern void prmsg(const char *s1, const char *s2, const char *s3);
extern void warning(const char *s1, const char *s2);

/*
 *	pp8.c
 */
extern EVALINT eval(void);
extern EVALINT evaltern(void);
extern EVALINT evallor(void);
extern EVALINT evalland(void);
extern EVALINT evalbor(void);
extern EVALINT evalbxor(void);
extern EVALINT evalband(void);
extern EVALINT evaleq(void);
extern EVALINT evalrel(void);
extern EVALINT evalsh(void);
extern EVALINT evalsum(void);
extern EVALINT evalmdr(void);
extern EVALINT evalfuns(void);
extern EVALINT evalucom(void);
extern EVALINT evalunot(void);
extern EVALINT evalumin(void);
extern EVALINT evalval(void);
extern EVALINT hexbin(char ch);
extern int ishex(char ch);
extern int isoct(char ch);
extern int item(int (*fun)(int), int f);
extern int look(const char *str);
extern int match(char *tbuf, const char *str);
extern char *readexpline(char *buf, int bufsize);
extern int test(const char *str);
