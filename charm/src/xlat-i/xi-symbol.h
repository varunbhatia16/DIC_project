#ifndef _SYMBOL_H
#define _SYMBOL_H

#include "xi-util.h"
#include "EToken.h"
#include "CEntry.h"
#include "sdag-globals.h"

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <list>


namespace xi {

// Reserved words
struct rwentry {
  const char *res;	int tok;
};

class Chare;
class CParsedFile;
class EncapState;

extern void generateVarSignature(XStr& decls, XStr& defs,
                              const Entry* entry, bool declareStatic,
                              const char* returnType, const XStr* name, bool isEnd,
                              std::list<CStateVar*>* params);
extern void generateVarSignature(XStr& decls, XStr& defs,
                              const Chare* chare, bool declareStatic,
                              const char* returnType, const XStr* name, bool isEnd,
                              std::list<CStateVar*>* params);
extern void generateClosureSignature(XStr& decls, XStr& defs,
                                     const Chare* chare, bool declareStatic,
                                     const char* returnType, const XStr* name, bool isEnd,
                                     std::list<EncapState*> params, int numRefs = 0);
extern void generateClosureSignature(XStr& decls, XStr& defs,
                                     const Entry* entry, bool declareStatic,
                                     const char* returnType, const XStr* name, bool isEnd,
                                     std::list<EncapState*> params, int numRefs = 0);
extern void endMethod(XStr& op);

class CStateVar;

/******************* Utilities ****************/

class Prefix {
public:
  static const char *Proxy;
  static const char *ProxyElement;
  static const char *ProxySection;
  static const char *Message;
  static const char *Index;
  static const char *Python;
};

typedef enum {
  forAll=0,forIndividual=1,forSection=2,forPython=3,forIndex=-1
} forWhom;

class Chare;//Forward declaration
class Message;
class TParamList;
extern int fortranMode;
extern int internalMode;
extern const char *cur_file;
void die(const char *why,int line=-1);

class Value : public Printable {
  private:
    int factor;
    const char *val;
  public:
    Value(const char *s);
    void print(XStr& str) { str << val; }
    int getIntVal(void);
};

class ValueList : public Printable {
  private:
    Value *val;
    ValueList *next;
  public:
    ValueList(Value* v, ValueList* n=0) : val(v), next(n) {}
    void print(XStr& str) {
      if(val) {
        str << "["; val->print(str); str << "]";
      }
      if(next)
        next->print(str);
    }
    void printValue(XStr& str) {
      if(val) {
        val->print(str);
      }
      if(next) {
	die("Unsupported type");
      }
    }
    void printValueProduct(XStr& str) {
      if (!val)
	die("Must have a value for an array dimension");

      str << "("; val->print(str); str << ")";
      if (next) {
	str << " * ";
	next->printValueProduct(str);
      }
    }
    void printZeros(XStr& str) {
      str << "[0]";
      if (next)
	next->printZeros(str);
    }
};

class Module;

class AstNode : public Printable {
protected:
  int line;
public:
  AstNode(int line_ = -1) : line(line_) { }
    virtual void outputClosuresDecl(XStr& str) { (void)str; }
    virtual void outputClosuresDef(XStr& str) { (void)str; }
    virtual void genDecls(XStr& str) { (void)str; }
    virtual void genDefs(XStr& str) { (void)str; }
    virtual void genClosureEntryDecls(XStr& str) { }
    virtual void genClosureEntryDefs(XStr& str) { }
    virtual void genReg(XStr& str) { (void)str; }
    virtual void genGlobalCode(XStr scope, XStr &decls, XStr &defs)
    { (void)scope; (void)decls; (void)defs; }
    virtual void preprocess() { }
    virtual void check() { }
    virtual void printChareNames() { }

    // DMK - Accel Support
    virtual int genAccels_spe_c_funcBodies(XStr& str) { (void)str; return 0; }
    virtual void genAccels_spe_c_regFuncs(XStr& str) { (void)str; }
    virtual void genAccels_spe_c_callInits(XStr& str) { (void)str; }
    virtual void genAccels_spe_h_includes(XStr& str) { (void)str; }
    virtual void genAccels_spe_h_fiCountDefs(XStr& str) { (void)str; }
    virtual void genAccels_ppe_c_regFuncs(XStr& str) { (void)str; }
};

class Construct : virtual public AstNode {
  protected:
    int external;
  public:
    Module *containerModule;
    Construct() {external=0;}
    void setExtern(int& e) { external = e; }
    void setModule(Module *m) { containerModule = m; }
};

template <typename Child>
class AstChildren : public virtual AstNode
{
protected:
  std::list<Child*> children;

public:
  AstChildren(int line_, Child *c, AstChildren *cs)
    : AstNode(line_)
    {
      children.push_back(c);
      if (cs)
	children.insert(children.end(), cs->children.begin(), cs->children.end());
    }

  template <typename T>
  AstChildren(std::list<T*>&l)
    {
      children.insert(children.begin(), l.begin(), l.end());
      l.clear();
    }
  void push_back(Child *c);

  void preprocess();
  void check();
  void print(XStr& str);

  void printChareNames();

  void outputClosuresDecl(XStr& str);
  void outputClosuresDef(XStr& str);

  void genClosureEntryDecls(XStr& str);
  void genClosureEntryDefs(XStr& str);
  void genDecls(XStr& str);
  void genDefs(XStr& str);
  void genReg(XStr& str);
  void genGlobalCode(XStr scope, XStr &decls, XStr &defs);

  // Accelerated Entry Method support
  int genAccels_spe_c_funcBodies(XStr& str);
  void genAccels_spe_c_regFuncs(XStr& str);
  void genAccels_spe_c_callInits(XStr& str);
  void genAccels_spe_h_includes(XStr& str);
  void genAccels_spe_h_fiCountDefs(XStr& str);
  void genAccels_ppe_c_regFuncs(XStr& str);

  template <typename T>
  void recurse(T arg, void (Child::*fn)(T));
  void recursev(void (Child::*fn)());
};

class ConstructList : public AstChildren<Construct>, public Construct {
public:
  ConstructList(int l, Construct *c, ConstructList *n=0)
    : AstChildren<Construct>(l, c, n) { }
};

/*********************** Type System **********************/
class Type : public Printable {
  public:
    virtual void print(XStr&) = 0;
    virtual int isVoid(void) const {return 0;}
    virtual int isBuiltin(void) const { return 0; }
    virtual int isMessage(void) const {return 0;}
    virtual int isTemplated(void) const { return 0; }
    virtual int isPointer(void) const {return 0;}
    virtual int isNamed(void) const { return 0; }
    virtual int isCkArgMsgPtr(void) const {return 0;}
    virtual int isCkArgMsg(void) const {return 0;}
    virtual int isCkMigMsgPtr(void) const {return 0;}
    virtual int isCkMigMsg(void) const {return 0;}
    virtual int isReference(void) const {return 0;}
    virtual int isInt(void) const { return 0; }
    virtual bool isConst(void) const {return false;}
    virtual Type *deref(void) {return this;}
    virtual const char *getBaseName(void) const = 0;
    virtual const char *getScope(void) const = 0;
    virtual int getNumStars(void) const {return 0;}
    virtual void genProxyName(XStr &str,forWhom forElement);
    virtual void genIndexName(XStr &str);
    virtual void genMsgProxyName(XStr& str);
    XStr proxyName(forWhom w)
    	{XStr ret; genProxyName(ret,w); return ret;}
    XStr indexName(void) 
    	{XStr ret; genIndexName(ret); return ret;}
    XStr msgProxyName(void) 
    	{XStr ret; genMsgProxyName(ret); return ret;}
    virtual void printVar(XStr &str, char *var) {print(str); str<<" "; str<<var;}
    int operator==(const Type &tp) const {
      return  (strcmp(getBaseName(), tp.getBaseName())==0);
    }
    virtual ~Type() { }
};

class BuiltinType : public Type {
  private:
    char *name;
  public:
    BuiltinType(const char *n) : name((char *)n) {}
    int isBuiltin(void) const {return 1;}
    void print(XStr& str) { str << name; }
    int isVoid(void) const { return !strcmp(name, "void"); }
    int isInt(void) const { return !strcmp(name, "int"); }
    const char *getBaseName(void) const { return name; }
    const char *getScope(void) const { return NULL; }
};

class NamedType : public Type {
  private:
    const char* name;
    const char* scope;
    TParamList *tparams;
  public:
    NamedType(const char* n, TParamList* t=0, const char* scope_=NULL)
       : name(n), scope(scope_), tparams(t) {}
    int isTemplated(void) const { return (tparams!=0); }
    int isCkArgMsg(void) const {return 0==strcmp(name,"CkArgMsg");}
    int isCkMigMsg(void) const {return 0==strcmp(name,"CkMigrateMessage");}
    void print(XStr& str);
    int isNamed(void) const {return 1;}
    virtual const char *getBaseName(void) const { return name; }
    virtual const char *getScope(void) const { return scope; }
    virtual void genProxyName(XStr& str,forWhom forElement);
    virtual void genIndexName(XStr& str);
    virtual void genMsgProxyName(XStr& str);
};

class PtrType : public Type {
  private:
    Type *type;
    int numstars; // level of indirection
  public:
    PtrType(Type *t) : type(t), numstars(1) {}
    int isPointer(void) const {return 1;}
    int isCkArgMsgPtr(void) const {return numstars==1 && type->isCkArgMsg();}
    int isCkMigMsgPtr(void) const {return numstars==1 && type->isCkMigMsg();}
    int isMessage(void) const {return numstars==1 && !type->isBuiltin();}
    void indirect(void) { numstars++; }
    int getNumStars(void) const {return numstars; }
    void print(XStr& str);
    Type* deref(void) { return type; }
    const char *getBaseName(void) const { return type->getBaseName(); }
    const char *getScope(void) const { return NULL; }
    virtual void genMsgProxyName(XStr& str) { 
      if(numstars != 1) {
        die("too many stars-- entry parameter must have form 'MTYPE *msg'"); 
      } else {
        type->genMsgProxyName(str);
      }
    }
};

class ReferenceType : public Type {
  private:
    Type *referant;
  public:
    ReferenceType(Type *t) : referant(t) {}
    int isReference(void) const {return 1;}
    void print(XStr& str) {str<<referant<<" &";}
    virtual Type *deref(void) {return referant;}
    const char *getBaseName(void) const { return referant->getBaseName(); }
    const char *getScope(void) const { return NULL; }
};

class ConstType : public Type {
private:
  Type *constType;
public:
  ConstType(Type *t) : constType(t) {}
  void print(XStr& str) {str << "const " << constType;}
  virtual bool isConst(void) const {return true;}
  virtual Type *deref(void) {return constType;}
  const char *getBaseName(void) const { return constType->getBaseName(); }
  const char *getScope(void) const { return NULL; }
};

//This is used as a list of base classes
class TypeList : public Printable {
    Type *type;
    TypeList *next;
  public:
    TypeList(Type *t, TypeList *n=0) : type(t), next(n) {}
    ~TypeList() { delete type; delete next; }
    int length(void) const;
    Type *getFirst(void) {return type;}
    void print(XStr& str);
    void genProxyNames(XStr& str, const char *prefix, const char *middle, 
                        const char *suffix, const char *sep, forWhom forElement);
};

/**************** Parameter types & lists (for marshalling) ************/
class Parameter {
public:
    Type *type;
    const char *name; /*The name of the variable, if any*/
    const char *given_name; /*The name of the msg in ci file, if any*/
    const char *arrLen; /*The expression for the length of the array;
    			 NULL if not an array*/
    Value *val; /*Initial value, if any*/
    int line;
    int byReference; //Fake a pass-by-reference (for efficiency)
    bool declaredReference; // Actually was declared a reference
    int conditional; //If the parameter is conditionally packed
    bool byConst;

    // DMK - Added field for accelerator options
    int accelBufferType;
    XStr* accelInstName;
    bool podType;

    friend class ParamList;
    void pup(XStr &str);
    void copyPtr(XStr &str);
    void marshallArraySizes(XStr &str);
    void marshallArrayData(XStr &str);
    void beginUnmarshall(XStr &str);
    void beginUnmarshallSDAGCall(XStr &str);
    void unmarshallArrayData(XStr &str);
    void unmarshallArrayDataSDAG(XStr &str);
    void unmarshallArrayDataSDAGCall(XStr &str);
    void pupAllValues(XStr &str);
  public:
    Entry *entry;
    Parameter(int Nline,Type *Ntype,const char *Nname=0,
    	const char *NarrLen=0,Value *Nvalue=0);
    void setConditional(int c) { conditional = c; if (c) byReference = false; };
    void print(XStr &str,int withDefaultValues=0,int useConst=1);
    void printAddress(XStr &str);
    void printValue(XStr &str);
    int isMessage(void) const {return type->isMessage();}
    int isVoid(void) const {return type->isVoid();}
    int isCkArgMsgPtr(void) const {return type->isCkArgMsgPtr();}
    int isCkMigMsgPtr(void) const {return type->isCkMigMsgPtr();}
    int isArray(void) const {return arrLen!=NULL;}
    int isConditional(void) const {return conditional;}
    Type *getType(void) {return type;}
    const char *getArrayLen(void) const {return arrLen;}
    const char *getGivenName(void) const {return given_name;}
    void setGivenName(const char* s) {given_name = s;}
    const char *getName(void) const {return name;}
    void printMsg(XStr& str) {
      type->print(str);
      if(given_name!=0)
        str <<given_name;
    }
    int operator==(const Parameter &parm) const {
      return *type == *parm.type;
    }

    // DMK - Added for accelerator options
    public:
    enum {
      ACCEL_BUFFER_TYPE_UNKNOWN   = 0,
      ACCEL_BUFFER_TYPE_MIN       = 1,
      ACCEL_BUFFER_TYPE_READWRITE = 1,
      ACCEL_BUFFER_TYPE_READONLY  = 2,
      ACCEL_BUFFER_TYPE_WRITEONLY = 3,
      ACCEL_BUFFER_TYPE_MAX       = 3
    };
    void setAccelBufferType(int abt) {
      accelBufferType = ((abt < ACCEL_BUFFER_TYPE_MIN || abt > ACCEL_BUFFER_TYPE_MAX) ? (ACCEL_BUFFER_TYPE_UNKNOWN) : (abt));
    }
    int getAccelBufferType() { return accelBufferType; }
    void setAccelInstName(XStr* ain) { accelInstName = ain; }
    XStr* getAccelInstName(void) { return accelInstName; }

};
class ParamList {
    typedef int (Parameter::*pred_t)(void) const;
    int orEach(pred_t f);
    typedef void (Parameter::*fn_t)(XStr &str);
    void callEach(fn_t f,XStr &str);
    bool manyPointers;
  public:
    Entry *entry;
    Parameter *param;
    ParamList *next;
    ParamList(ParamList *pl) : manyPointers(false), param(pl->param), next(pl->next) {}
    ParamList(Parameter *Nparam,ParamList *Nnext=NULL)
      :param(Nparam), next(Nnext) { 
          manyPointers = false;
          if(next != NULL && (param->isMessage() || next->isMessage())){
            manyPointers = true;
          }
    }
    void print(XStr &str,int withDefaultValues=0,int useConst=1);
    void printAddress(XStr &str);
    void printValue(XStr &str);
    int isNamed(void) const {return param->type->isNamed();}
    int isBuiltin(void) const {return param->type->isBuiltin();}
    int isMessage(void) const {
    	return (next==NULL) && param->isMessage();
    }
    const char *getArrayLen(void) const {return param->getArrayLen();}
    int isArray(void) const {return param->isArray();}
    int isReference(void) const {return param->type->isReference() || param->byReference;}
    int declaredReference(void) const {return param->type->isReference() || param->declaredReference; }
    bool isConst(void) const {return param->type->isConst() || param->byConst;}
    int isVoid(void) const {
    	return (next==NULL) && param->isVoid();
    }
    int isPointer(void) const {return param->type->isPointer();}
    const char *getGivenName(void) const {return param->getGivenName();}
    void setGivenName(const char* s) {param->setGivenName(s);}
    const char *getName(void) const {return param->getName();}
    int isMarshalled(void) const {
    	return !isVoid() && !isMessage();
    }
    int isCkArgMsgPtr(void) const {
        return (next==NULL) && param->isCkArgMsgPtr();
    }
    int isCkMigMsgPtr(void) const {
        return (next==NULL) && param->isCkMigMsgPtr();
    }
    int getNumStars(void) const {return param->type->getNumStars(); }
    const char *getBaseName(void) {
    	return param->type->getBaseName();
    }
    void genMsgProxyName(XStr &str) {
    	param->type->genMsgProxyName(str);
    }
    void printMsg(XStr& str) {
        ParamList *pl;
        param->printMsg(str);
        pl = next;
        while (pl != NULL)
        {
           str <<", ";
           pl->param->printMsg(str);
           pl = pl->next;
        } 
    }
    void preprocess();
    int hasConditional();
    void marshall(XStr &str, XStr &entry);
    void beginUnmarshall(XStr &str);
    void beginUnmarshallSDAG(XStr &str);
    void beginUnmarshallSDAGCall(XStr &str, bool usesImplBuf);
    void beginRednWrapperUnmarshall(XStr &str, bool isSDAGGen);
    void unmarshall(XStr &str, int isFirst=1);
    void unmarshallSDAGCall(XStr &str, int isFirst=1);
    void unmarshallAddress(XStr &str, int isFirst=1);
    void pupAllValues(XStr &str);
    void endUnmarshall(XStr &str);
    int operator==(ParamList &plist) {
      if (!(*param == *(plist.param))) return 0;
      if (!next && !plist.next) return 1;
      if (!next || !plist.next) return 0;
      return *next ==  *plist.next;
    }
    void checkParamList();
};

class FuncType : public Type {
  private:
    Type *rtype;
    const char *name;
    ParamList *params;
  public:
    FuncType(Type* r, const char* n, ParamList* p)
    	:rtype(r),name(n),params(p) {}
    void print(XStr& str) { 
      rtype->print(str);
      str << "(*" << name << ")(";
      if(params)
        params->print(str);
    }
    const char *getBaseName(void) const { return name; }
    const char *getScope(void) const { return NULL; }
};

/****************** Template Support **************/
/* Template Instantiation Parameter */
class TParam : public Printable {
  public:
    virtual void genSpec(XStr& str)=0;
};

/* List of Template Instantiation parameters */
class TParamList : public Printable {
    TParam *tparam;
    TParamList *next;
  public:
    TParamList(TParam *t, TParamList *n=0) : tparam(t), next(n) {}
    void print(XStr& str);
    void genSpec(XStr& str);
    std::string to_string();
};

/* A type instantiation parameter */
class TParamType : public TParam {
  Type *type;
  public:
    TParamType(Type *t) : type(t) {}
    void print(XStr& str) { type->print(str); }
    void genSpec(XStr& str) { type->print(str); }
};

/* A Value instantiation parameter */
class TParamVal : public TParam {
    const char *val;
  public:
    TParamVal(const char *v) : val(v) {}
    void print(XStr& str) { str << val; }
    void genSpec(XStr& str) { str << val; }
};

class Scope : public ConstructList {
  protected:
    const char* name_;
  public:
    Scope(const char* name, ConstructList* contents)
      : name_(name)
      , ConstructList(-1, NULL, contents)
    { }
    void genDecls(XStr& str) {
        str << "namespace " << name_ << " {\n";
        AstChildren<Construct>::genDecls(str);
        str << "} // namespace " << name_ << "\n";
    }
    void genDefs(XStr& str) {
        str << "namespace " << name_ << " {\n";
        AstChildren<Construct>::genDefs(str);
        str << "} // namespace " << name_ << "\n";
    }
    void genReg(XStr& str) {
        str << "using namespace " << name_ << ";\n";
        AstChildren<Construct>::genReg(str);
    }
    void genGlobalCode(XStr scope, XStr &decls, XStr &defs) {
      scope << name_ << "::";
      AstChildren<Construct>::genGlobalCode(scope, decls, defs);
    }
    void print(XStr& str) {
        str << "namespace " << name_ << "{\n";
        AstChildren<Construct>::print(str);
        str << "} // namespace " << name_ << "\n";
    }
    void outputClosuresDecl(XStr& str) {
      str << "namespace " << name_ << " {\n";
      AstChildren<Construct>::outputClosuresDecl(str);
      str << "} // namespace " << name_ << "\n";
    }
    void outputClosuresDef(XStr& str) {
      str << "namespace " << name_ << " {\n";
      AstChildren<Construct>::outputClosuresDef(str);
      str << "} // namespace " << name_ << "\n";
    }
};

class UsingScope : public Construct {
  protected:
    const char* name_;
    bool symbol_;
  public:
    UsingScope(const char* name, bool symbol=false) : name_(name), symbol_(symbol) {}
    virtual void genDecls(XStr& str) {
        str << "using ";
        if (!symbol_) str << "namespace ";
        str << name_ << ";\n";
    }
    virtual void print(XStr& str) {
        str << "using ";
        if (!symbol_) str << "namespace ";
        str << name_ << ";\n";
    }
};


/* A template construct */
class TVarList;
class TEntity;

class Template : public Construct {
    TVarList *tspec;
    TEntity *entity;
  public:
    Template(TVarList *t, TEntity *e) : tspec(t), entity(e) {}
    virtual void setExtern(int e);
    void print(XStr& str);
    void genDecls(XStr& str);
    void genDefs(XStr& str);
    void genSpec(XStr& str);
    void genVars(XStr& str);
    void outputClosuresDecl(XStr& str);
    void outputClosuresDef(XStr& str);

    // DMK - Accel Support
    int genAccels_spe_c_funcBodies(XStr& str);
    void genAccels_spe_c_regFuncs(XStr& str);
    void genAccels_spe_c_callInits(XStr& str);
    void genAccels_spe_h_includes(XStr& str);
    void genAccels_spe_h_fiCountDefs(XStr& str);
    void genAccels_ppe_c_regFuncs(XStr& str);
};

/* An entity that could be templated, i.e. chare, group or a message */
class TEntity : public Construct {
  protected:
    Template *templat;
  public:
    void setTemplate(Template *t) { templat = t; }
    virtual XStr tspec(void) const {
    	XStr str; 
    	if (templat) templat->genSpec(str); 
    	return str;
    }
    virtual XStr tvars(void) const {
    	XStr str;
    	if (templat) templat->genVars(str); 
    	return str;
    }
};
/* A formal argument of a template */
class TVar : public Printable {
  public:
    virtual void genLong(XStr& str) = 0;
    virtual void genShort(XStr& str) = 0;
};

/* a formal type argument */
class TType : public TVar {
    Type *type;
    Type *init;
  public:
    TType(Type *t, Type *i=0) : type(t), init(i) {}
    void print(XStr& str);
    void genLong(XStr& str);
    void genShort(XStr& str);
};

/* a formal function argument */
class TFunc : public TVar {
    FuncType *type;
    const char *init;
  public:
    TFunc(FuncType *t, const char *v=0) : type(t), init(v) {}
    void print(XStr& str) { type->print(str); if(init) str << "=" << init; }
    void genLong(XStr& str){ type->print(str); if(init) str << "=" << init; }
    void genShort(XStr& str) {str << type->getBaseName(); }
};

/* A formal variable argument */
class TName : public TVar {
    Type *type;
    const char *name;
    const char *val;
  public:
    TName(Type *t, const char *n, const char *v=0) : type(t), name(n), val(v) {}
    void print(XStr& str);
    void genLong(XStr& str);
    void genShort(XStr& str);
};

/* A list of formal arguments to a template */
class TVarList : public Printable {
    TVar *tvar;
    TVarList *next;
  public:
    TVarList(TVar *v, TVarList *n=0) : tvar(v), next(n) {}
    void print(XStr& str);
    void genLong(XStr& str);
    void genShort(XStr& str);
};

/******************* Chares, Arrays, Groups ***********/

struct SdagCollection
{
  CParsedFile *pf;
  bool sdagPresent;
  SdagCollection(CParsedFile *p) : pf(p), sdagPresent(false) {}
  void addNode(Entry *e);
};

/* Member of a chare or group, i.e. entry, RO or ROM */
class Member : public Construct {
   //friend class CParsedFile;
  protected:
    Chare *container;
  public:
    TVarList *tspec;
    Member() : container(0), tspec(0) { }
    inline Chare *getContainer() const { return container; }
    virtual void setChare(Chare *c) { container = c; }
    virtual int isSdag(void) { return 0; }
    virtual void collectSdagCode(SdagCollection *) { return; }
    XStr makeDecl(const XStr &returnType,int forProxy=0, bool isStatic = false);
    virtual void genPythonDecls(XStr& ) {}
    virtual void genIndexDecls(XStr& ) {}
    virtual void genPythonDefs(XStr& ) {}
    virtual void genPythonStaticDefs(XStr&) {}
    virtual void genPythonStaticDocs(XStr&) {}
    virtual void lookforCEntry(CEntry *)  {}
};

/* Chare or group is a templated entity */
class Chare : public TEntity {
  AstChildren<Member> *list;
  public:
    enum { //Set these attribute bits in "attrib"
    	CMIGRATABLE=1<<2,
	CPYTHON=1<<3,
    	CCHARE=1<<9,                 // plain non-migratable chare
    	CMAINCHARE=1<<10,
    	CARRAY=1<<11,
    	CGROUP=1<<12,
    	CNODEGROUP=1<<13
    };
    typedef unsigned int attrib_t;
    XStr sdagPUPReg;
    XStr sdagDefs, closuresDecl, closuresDef;
    NamedType *type;
  protected:
    attrib_t attrib;
    int hasElement;//0-- no element type; 1-- has element type
    forWhom forElement;
    int hasSection; //1-- applies only to array section

    TypeList *bases; //Base classes used by proxy
    TypeList *bases_CBase; //Base classes used by CBase (or NULL)

    int entryCount;
    int hasSdagEntry;

    void genTypedefs(XStr& str);
    void genRegisterMethodDef(XStr& str);
    void sharedDisambiguation(XStr &str,const XStr &superclass);
    void genMemberDecls(XStr &str);
  public:
    Chare(int ln, attrib_t Nattr, NamedType *t, TypeList *b=0, AstChildren<Member> *l=0);
    void genProxyNames(XStr& str, const char *prefix, const char *middle, 
                        const char *suffix, const char *sep);
    void genIndexNames(XStr& str, const char *prefix, const char *middle, 
                        const char *suffix, const char *sep);
    void printChareNames();
    XStr proxyName(int withTemplates=1); 
    XStr indexName(int withTemplates=1); 
    XStr indexList();
    XStr baseName(int withTemplates=1) const
    {
    	XStr str;
    	str<<type->getBaseName();
    	if (withTemplates) str<<tvars();
    	return str;
    }
    int  isTemplated(void) { return (templat!=0); }
    bool isTemplateDeclaration() { return templat; }
    bool isTemplateInstantiation() { return type->isTemplated(); }
    int  isMigratable(void) { return attrib&CMIGRATABLE; }
    int  isPython(void) { return attrib&CPYTHON; }
    int  isMainChare(void) {return attrib&CMAINCHARE;}
    int  isChare(void) {return attrib&CCHARE;}     // plain non-migratable chare
    int  isArray(void) {return attrib&CARRAY;}
    int  isGroup(void) {return attrib&CGROUP;}
    int  isNodeGroup(void) {return attrib&CNODEGROUP;}
    int  isForElement(void) const {return forElement==forIndividual;}
    int  isForSection(void) const {return forElement==forSection;}
    int  hasSdag() const { return hasSdagEntry; }
    void  setSdag(int f) { hasSdagEntry = f; }
    forWhom getForWhom(void) const {return forElement;}
    void print(XStr& str);
    void check();
    void genDefs(XStr& str);
    void genReg(XStr& str);
    void genDecls(XStr &str);
    void preprocess();

    // DMK - Accel Support
    int genAccels_spe_c_funcBodies(XStr& str) {
      int rtn = 0;
      if (list) { rtn += list->genAccels_spe_c_funcBodies(str); }
      return rtn;
    }
    void genAccels_spe_c_regFuncs(XStr& str) {
      if (list) { list->genAccels_spe_c_regFuncs(str); }
    }
    void genAccels_spe_c_callInits(XStr& str) {
      if (list) { list->genAccels_spe_c_callInits(str); }
    }
    void genAccels_spe_h_includes(XStr& str) {
      if (list) { list->genAccels_spe_h_includes(str); }
    }
    void genAccels_spe_h_fiCountDefs(XStr& str) {
      if (list) { list->genAccels_spe_h_fiCountDefs(str); }
    }
    void genAccels_ppe_c_regFuncs(XStr& str) {
      if (list) { list->genAccels_ppe_c_regFuncs(str); }
    }

    int nextEntry(void) {return entryCount++;}
    virtual void genSubDecls(XStr& str);
    virtual void outputClosuresDecl(XStr& str);
    virtual void outputClosuresDef(XStr& str);
    virtual void genClosureEntryDecls(XStr& str);
    virtual void genClosureEntryDefs(XStr& str);
    void genPythonDecls(XStr& str);
    void genPythonDefs(XStr& str);
    virtual char *chareTypeName(void) {return (char *)"chare";}
    virtual char *proxyPrefix(void);
    virtual void genSubRegisterMethodDef(XStr& str) { (void)str; }
    void lookforCEntry(CEntry *centry);
};

class MainChare : public Chare {
  public:
    MainChare(int ln, attrib_t Nattr,
              NamedType *t, TypeList *b=0, AstChildren<Member> *l=0):
	    Chare(ln, Nattr|CMAINCHARE, t,b,l) {}
    virtual char *chareTypeName(void) {return (char *) "mainchare";}
};

class Array : public Chare {
  protected:
    XStr indexSuffix;
    XStr indexType;//"CkArrayIndex"+indexSuffix;
  public:
    Array(int ln, attrib_t Nattr, NamedType *index,
          NamedType *t, TypeList *b=0, AstChildren<Member> *l=0);
    virtual int is1D(void) {return indexSuffix==(const char*)"1D";}
    virtual const char* dim(void) {return indexSuffix.get_string_const();}
    virtual void genSubDecls(XStr& str);
    virtual char *chareTypeName(void) {return (char *) "array";}
};

class Group : public Chare {
  public:
    Group(int ln, attrib_t Nattr,
          NamedType *t, TypeList *b=0, AstChildren<Member> *l=0);
    virtual void genSubDecls(XStr& str);
    virtual char *chareTypeName(void) {return (char *) "group";}
    virtual void genSubRegisterMethodDef(XStr& str);
};

class NodeGroup : public Group {
  public:
    NodeGroup(int ln, attrib_t Nattr,
              NamedType *t, TypeList *b=0, AstChildren<Member> *l=0):
	    Group(ln,Nattr|CNODEGROUP,t,b,l) {}
    virtual char *chareTypeName(void) {return (char *) "nodegroup";}
};


/****************** Messages ***************/
class Message; // forward declaration

class MsgVar {
 public:
  Type *type;
  const char *name;
  int cond;
  int array;
  MsgVar(Type *t, const char *n, int c, int a) : type(t), name(n), cond(c), array(a) { }
  Type *getType() { return type; }
  const char *getName() { return name; }
  int isConditional() { return cond; }
  int isArray() { return array; }
  void print(XStr &str) {str<<(isConditional()?"conditional ":"");type->print(str);str<<" "<<name<<(isArray()?"[]":"")<<";";}
};

class MsgVarList : public Printable {
 public:
  MsgVar *msg_var;
  MsgVarList *next;
  MsgVarList(MsgVar *mv, MsgVarList *n=0) : msg_var(mv), next(n) {}
  void print(XStr &str) {
    msg_var->print(str);
    str<<"\n";
    if(next) next->print(str);
  }
  int len(void) { return (next==0)?1:(next->len()+1); }
};

class Message : public TEntity {
    NamedType *type;
    MsgVarList *mvlist;
    void printVars(XStr& str) {
      if(mvlist!=0) {
        str << "{\n";
        mvlist->print(str);
        str << "}\n";
      }
    }
  public:
    Message(int l, NamedType *t, MsgVarList *mv=0)
      : type(t), mvlist(mv) 
      { line=l; setTemplate(0); }
    void print(XStr& str);
    void genDecls(XStr& str);
    void genDefs(XStr& str);
    void genReg(XStr& str);

    virtual const char *proxyPrefix(void) {return Prefix::Message;}
    void genAllocDecl(XStr& str);
    int numArrays(void) {
      if (mvlist==0) return 0;
      int count = 0;
      MsgVarList *mv = mvlist;
      for (int i=0; i<mvlist->len(); ++i, mv=mv->next) if (mv->msg_var->isArray()) count ++;
      return count;
    }
    int numConditional(void) {
      if (mvlist==0) return 0;
      int count = 0;
      MsgVarList *mv = mvlist;
      for (int i=0; i<mvlist->len(); ++i, mv=mv->next) if (mv->msg_var->isConditional()) count ++;
      return count;
    }
    int numVars(void) { return ((mvlist==0) ? 0 : mvlist->len()); }
};





/******************* Entry Point ****************/
// Entry attributes
#define STHREADED 0x01
#define SSYNC     0x02
#define SLOCKED   0x04
#define SPURE     0x10
#define SMIGRATE  0x20 //<- is magic migration constructor
#define SCREATEHERE   0x40 //<- is a create-here-if-nonexistant
#define SCREATEHOME   0x80 //<- is a create-at-home-if-nonexistant
#define SIMMEDIATE    0x100 //<- is a immediate
#define SNOKEEP       0x200
#define SNOTRACE      0x400
#define SSKIPSCHED    0x800 //<- is a message skipping charm scheduler
#define SPYTHON       0x1000
#define SINLINE       0x2000 //<- inline message
#define SIGET         0x4000 
#define SLOCAL        0x8000 //<- local message
#define SACCEL        0x10000
#define SMEM          0x20000
#define SREDUCE       0x40000 // <- reduction target
#define SAPPWORK      0x80000 // <- reduction target

/* An entry construct */
class Entry : public Member {
public:
    XStr* genClosureTypeName;
    XStr* genClosureTypeNameProxy;
    XStr* genClosureTypeNameProxyTemp;
    int line,entryCount;
  private:    
    int attribs;    
    Type *retType;
    Value *stacksize;
    const char *pythonDoc;
    

public:
    XStr proxyName(void) {return container->proxyName();}
    XStr indexName(void) {return container->indexName();}

private:
//    friend class CParsedFile;
    int hasCallMarshall;
    void genCall(XStr &dest,const XStr &preCall, bool redn_wrapper=false,
                 bool usesImplBuf = false);

    XStr epStr(bool isForRedn = false, bool templateCall = false);
    XStr epIdx(int fromProxy=1, bool isForRedn = false);
    XStr epRegFn(int fromProxy=1, bool isForRedn = false);
    XStr chareIdx(int fromProxy=1);
    void genEpIdxDecl(XStr& str);
    void genEpIdxDef(XStr& str);

    void genClosure(XStr& str, bool isDef);
    void genClosureEntryDefs(XStr& str);
    void genClosureEntryDecls(XStr& str);
    
    void genChareDecl(XStr& str);
    void genChareStaticConstructorDecl(XStr& str);
    void genChareStaticConstructorDefs(XStr& str);
    void genChareDefs(XStr& str);
    
    void genArrayDefs(XStr& str);
    void genArrayStaticConstructorDecl(XStr& str);
    void genArrayStaticConstructorDefs(XStr& str);
    void genArrayDecl(XStr& str);
    
    void genGroupDecl(XStr& str);
    void genGroupStaticConstructorDecl(XStr& str);
    void genGroupStaticConstructorDefs(XStr& str);
    void genGroupDefs(XStr& str);
    
    void genPythonDecls(XStr& str);
    void genPythonDefs(XStr& str);
    void genPythonStaticDefs(XStr& str);
    void genPythonStaticDocs(XStr& str);

    // DMK - Accel Support
    void genAccelFullParamList(XStr& str, int makeRefs);
    void genAccelFullCallList(XStr& str);
    void genAccelIndexWrapperDecl_general(XStr& str);
    void genAccelIndexWrapperDef_general(XStr& str);
    void genAccelIndexWrapperDecl_spe(XStr& str);
    void genAccelIndexWrapperDef_spe(XStr& str);
    int genAccels_spe_c_funcBodies(XStr& str);
    void genAccels_spe_c_regFuncs(XStr& str);
    void genAccels_ppe_c_regFuncs(XStr& str);

    XStr paramType(int withDefaultVals,int withEO=0,int useConst=1);
    XStr paramComma(int withDefaultVals,int withEO=0);
    XStr eo(int withDefaultVals,int priorComma=1);
    XStr syncReturn(void);
    XStr marshallMsg(void);
    XStr callThread(const XStr &procName,int prependEntryName=0);

    // SDAG support
    std::list<CStateVar *> estateVars;

  public:
    XStr *label;
    char *name;
    TParamList *targs;

    // SDAG support
    SdagConstruct *sdagCon;
    std::list<CStateVar *> stateVars;
    CEntry *entryPtr;
    const char *intExpr;
    ParamList *param;
    int isWhenEntry;

    void addEStateVar(CStateVar *sv) {
      estateVars.push_back(sv);
      stateVars.push_back(sv);
    }

    // DMK - Accel Support
    ParamList* accelParam;
    XStr* accelCodeBody;
    XStr* accelCallbackName;
    void setAccelParam(ParamList* apl) { accelParam = apl; }
    void setAccelCodeBody(XStr* acb) { accelCodeBody = acb; }
    void setAccelCallbackName(XStr* acbn) { accelCallbackName = acbn; }

    // DMK - Accel Support
    int accel_numScalars;
    int accel_numArrays;
    int accel_dmaList_numReadOnly;
    int accel_dmaList_numReadWrite;
    int accel_dmaList_numWriteOnly;
    int accel_dmaList_scalarNeedsWrite;

    Entry(int l, int a, Type *r, const char *n, ParamList *p, Value *sz=0, SdagConstruct *sc =0, const char *e=0);
    void setChare(Chare *c);
    int paramIsMarshalled(void) { return param->isMarshalled(); }
    int getStackSize(void) { return (stacksize ? stacksize->getIntVal() : 0); }
    int isThreaded(void) { return (attribs & STHREADED); }
    int isSync(void) { return (attribs & SSYNC); }
    int isIget(void) { return (attribs & SIGET); }
    int isConstructor(void) { return !strcmp(name, container->baseName(0).get_string());}
    bool isMigrationConstructor() { return isConstructor() && (attribs & SMIGRATE); }
    int isExclusive(void) { return (attribs & SLOCKED); }
    int isImmediate(void) { return (attribs & SIMMEDIATE); }
    int isSkipscheduler(void) { return (attribs & SSKIPSCHED); }
    int isInline(void) { return attribs & SINLINE; }
    int isLocal(void) { return attribs & SLOCAL; }
    int isCreate(void) { return (attribs & SCREATEHERE)||(attribs & SCREATEHOME); }
    int isCreateHome(void) { return (attribs & SCREATEHOME); }
    int isCreateHere(void) { return (attribs & SCREATEHERE); }
    int isPython(void) { return (attribs & SPYTHON); }
    int isNoTrace(void) { return (attribs & SNOTRACE); }
    int isAppWork(void) { return (attribs & SAPPWORK); }
    int isNoKeep(void) { return (attribs & SNOKEEP); }
    int isSdag(void) { return (sdagCon!=0); }

    // DMK - Accel support
    int isAccel(void) { return (attribs & SACCEL); }

    int isMemCritical(void) { return (attribs & SMEM); }
    int isReductionTarget(void) { return (attribs & SREDUCE); }

    void print(XStr& str);
    void check();
    void genIndexDecls(XStr& str);
    void genDecls(XStr& str);
    void genDefs(XStr& str);
    void genReg(XStr& str);
    XStr genRegEp(bool isForRedn = false);
    void preprocess();
    char *getEntryName() { return name; }
    void generateEntryList(std::list<CEntry*>&, WhenConstruct *);
    void collectSdagCode(SdagCollection *sc);
    void propagateState(int);
    void lookforCEntry(CEntry *centry);
    int getLine() { return line; }
};

class EntryList {
  public:
    Entry *entry;
    EntryList *next;
    EntryList(Entry *e,EntryList *elist=NULL):
    	entry(e), next(elist) {}
    void generateEntryList(std::list<CEntry*>&, WhenConstruct *);
};


/******************** AccelBlock : Block of code for accelerator **********************/
class AccelBlock : public Construct {

 protected:

  XStr* code;

 private:

  void outputCode(XStr& str) {
    if (code != NULL) {
      str << "\n";
      templateGuardBegin(false, str);
      str << "/***** Accel_Block Start *****/\n"
          << (*(code))
          << "\n/***** Accel_Block End *****/\n";
      templateGuardEnd(str);
      str << "\n";
    }
  }

 public:

  /// Constructor(s)/Destructor ///
  AccelBlock(int l, XStr* c) { line = l; code = c; }
  ~AccelBlock() { delete code; }

  /// Printable Methods ///
  void print(XStr& str) { (void)str; }

  /// Construct Methods ///
  void genDefs(XStr& str) { outputCode(str); }

  /// Construct Accel Support Methods ///
  int genAccels_spe_c_funcBodies(XStr& str) { outputCode(str); return 0; }
};


/****************** Modules, etc. ****************/
class Module : public Construct {
    int _isMain;
    const char *name;
    ConstructList *clist;

  public:
    Module(int l, const char *n, ConstructList *c);
    void print(XStr& str);
    void printChareNames() { if (clist) clist->printChareNames(); }
    void check();
    void generate();
    void setModule();
    void prependConstruct(Construct *c) { clist = new ConstructList(-1, c, clist); }
    void preprocess();
    void genDepend(const char *cifile);
    void genDecls(XStr& str);
    void genDefs(XStr& str);
    void genReg(XStr& str);
    void setMain(void) { _isMain = 1; }
    int isMain(void) { return _isMain; }

    // DMK - Accel Support
    int genAccels_spe_c_funcBodies(XStr& str);
    void genAccels_spe_c_regFuncs(XStr& str);
    void genAccels_spe_c_callInits(XStr& str);
    void genAccels_spe_h_includes(XStr& str);
    void genAccels_spe_h_fiCountDefs(XStr& str);
    void genAccels_ppe_c_regFuncs(XStr& str);
};

class Readonly : public Member {
    int msg; // is it a readonly var(0) or msg(1) ?
    Type *type;
    const char *name;
    ValueList *dims;
    XStr qName(void) const { /*Return fully qualified name*/
      XStr ret;
      if(container) ret<<container->baseName()<<"::";
      ret<<name;
      return ret;
    }
  public:
    Readonly(int l, Type *t, const char *n, ValueList* d, int m=0)
	    : msg(m), type(t), name(n)
            { line=l; dims=d; setChare(0); }
    void print(XStr& str);
    void genDecls(XStr& str);
    void genIndexDecls(XStr& str);
    void genDefs(XStr& str);
    void genReg(XStr& str);
};

class InitCall : public Member {

    const char *name; //Name of subroutine to call
    int isNodeCall;

    // DMK - Accel Support
    int isAccelFlag;

public:

    InitCall(int l, const char *n, int nodeCall);
    void print(XStr& str);
    void genReg(XStr& str);

    // DMK - Accel Support
    void genAccels_spe_c_callInits(XStr& str);

    void setAccel() { isAccelFlag = 1; }
    void clearAccel() { isAccelFlag = 0; }
    int isAccel() { return isAccelFlag; }
};

class PUPableClass : public Member {
    NamedType* type;
    PUPableClass *next; //Linked-list of PUPable classes
public:
    PUPableClass(int l, NamedType* type_, PUPableClass *next_);
    void print(XStr& str);
    void genDefs(XStr& str);
    void genReg(XStr& str);

    // DMK - Accel Support
    int genAccels_spe_c_funcBodies(XStr& str) {
      int rtn=0;
      if (next) { rtn += next->genAccels_spe_c_funcBodies(str); }
      return rtn;
    }
    void genAccels_spe_c_regFuncs(XStr& str) {
      if (next) { next->genAccels_spe_c_regFuncs(str); }
    }
    void genAccels_spe_c_callInits(XStr& str) {
      if (next) { next->genAccels_spe_c_callInits(str); }
    }
    void genAccels_spe_h_includes(XStr& str) {
      if (next) { next->genAccels_spe_h_includes(str); }
    }
    void genAccels_spe_h_fiCountDefs(XStr& str) {
      if (next) { next->genAccels_spe_h_fiCountDefs(str); }
    }
    void genAccels_ppe_c_regFuncs(XStr& str) {
      if (next) { next->genAccels_ppe_c_regFuncs(str); }
    }
};

class IncludeFile : public Member {
    const char *name; //Name of include file
public:
    IncludeFile(int l, const char *name_);
    void print(XStr& str);
    void genDecls(XStr& str);
};

class ClassDeclaration : public Member {
    const char *name; //Name of class 
public:
    ClassDeclaration(int l, const char *name_);
    void print(XStr& str);
    void genDecls(XStr& str);
};


/******************* Structured Dagger Constructs ***************/
class SdagConstruct { 
private:
  void generateOverlap(XStr& decls, XStr& defs, Entry* entry);
  void generateWhile(XStr& decls, XStr& defs, Entry* entry);
  void generateFor(XStr& decls, XStr& defs, Entry* entry);
  void generateIf(XStr& decls, XStr& defs, Entry* entry);
  void generateElse(XStr& decls, XStr& defs, Entry* entry);
  void generateForall(XStr& decls, XStr& defs, Entry* entry);
  void generateOlist(XStr& decls, XStr& defs, Entry* entry);
  void generateSdagEntry(XStr& decls, XStr& defs, Entry *entry);
  void generateSlist(XStr& decls, XStr& defs, Entry* entry);
  void generateCaseList(XStr& decls, XStr& defs, Entry* entry);

protected:
  void generateCall(XStr& op, std::list<EncapState*>& cur,
                    std::list<EncapState*>& next, const XStr* name,
                    const char* nameSuffix = 0);
  void generateTraceBeginCall(XStr& defs, int indent);          // for trace
  void generateBeginTime(XStr& defs);               //for Event Bracket
  void generateEventBracket(XStr& defs, int eventType);     //for Event Bracket
  void generateListEventBracket(XStr& defs, int eventType);
  void generateChildrenCode(XStr& decls, XStr& defs, Entry* entry);
  void generateChildrenEntryList(std::list<CEntry*>& CEntrylist, WhenConstruct *thisWhen);
  void propagateStateToChildren(std::list<EncapState*>, std::list<CStateVar*>&, std::list<CStateVar*>&, int);
  std::list<SdagConstruct *> *constructs;
  std::list<CStateVar *> *stateVars;
  std::list<EncapState*> encapState, encapStateChild;
  std::list<CStateVar *> *stateVarsChildren;

public:
  int unravelClosuresBegin(XStr& defs, bool child = false);
  void unravelClosuresEnd(XStr& defs, bool child = false);

  int nodeNum;
  XStr *label;
  XStr *counter;
  EToken type;
  char nameStr[128];
  XStr *traceName;	
  SdagConstruct *next;
  ParamList *param;
  //cppcheck-suppress unsafeClassCanLeak
  XStr *text;
  int nextBeginOrEnd;
  EntryList *elist;
  Entry* entry;
  SdagConstruct *con1, *con2, *con3, *con4;
  SdagConstruct(EToken t, SdagConstruct *construct1);

  SdagConstruct(EToken t, SdagConstruct *construct1, SdagConstruct *aList);

  SdagConstruct(EToken t, XStr *txt, SdagConstruct *c1, SdagConstruct *c2, SdagConstruct *c3,
              SdagConstruct *c4, SdagConstruct *constructAppend, EntryList *el);

  SdagConstruct(EToken t, const char *str) : type(t), traceName(NULL), con1(0), con2(0), con3(0), con4(0), elist(0)
  { text = new XStr(str); constructs = new std::list<SdagConstruct*>(); }
                                             
 
  SdagConstruct(EToken t) : type(t), traceName(NULL), con1(0), con2(0), con3(0), con4(0), elist(0)
  { constructs = new std::list<SdagConstruct*>(); }

  SdagConstruct(EToken t, XStr *txt) : type(t), traceName(NULL), text(txt), con1(0), con2(0), con3(0), con4(0), elist(0)
  { constructs = new std::list<SdagConstruct*>();  }

  virtual ~SdagConstruct();

  void init(EToken& t);
  SdagConstruct(EToken t, const char *entryStr, const char *codeStr, ParamList *pl);
  void numberNodes(void);
  void labelNodes();
  XStr* createLabel(const char* str, int nodeNum);
  virtual void generateEntryList(std::list<CEntry*>&, WhenConstruct *);
  void propagateState(int);
  virtual void propagateState(std::list<EncapState*>, std::list<CStateVar*>&, std::list<CStateVar*>&, int);
  virtual void generateCode(XStr& decls, XStr& defs, Entry *entry);
  void setNext(SdagConstruct *, int);
  void buildTypes(std::list<EncapState*>& state);

  // for trace
  virtual void generateTrace();
  void generateRegisterEp(XStr& defs);
  void generateTraceEp(XStr& decls, XStr& defs, Chare* chare);
  static void generateTraceEndCall(XStr& defs, int indent);
  static void generateTlineEndCall(XStr& defs);
  static void generateBeginExec(XStr& defs, const char *name);
  static void generateEndExec(XStr& defs);
  static void generateEndSeq(XStr& defs);
  static void generateDummyBeginExecute(XStr& defs, int indent);
};

class WhenConstruct : public SdagConstruct {
public:
  CStateVar* speculativeState;
  void generateCode(XStr& decls, XStr& defs, Entry *entry);
  WhenConstruct(EntryList *el, SdagConstruct *body)
    : SdagConstruct(SWHEN, 0, 0, 0,0,0, body, el)
    , speculativeState(0)
  { }
  void generateEntryList(std::list<CEntry*>& CEntrylist, WhenConstruct *thisWhen);
  void propagateState(std::list<EncapState*>, std::list<CStateVar*>&, std::list<CStateVar*>&, int);
  void generateEntryName(XStr& defs, Entry* e, int curEntry);
  void generateWhenCode(XStr& op, int indent);
};

extern void RemoveSdagComments(char *);

void generateLocalWrapper(XStr& decls, XStr& defs, int isVoid, XStr& signature, Entry* entry,
                          std::list<CStateVar*>* params, XStr* next);

class AtomicConstruct : public SdagConstruct {
public:
  void propagateState(std::list<EncapState*>, std::list<CStateVar*>&, std::list<CStateVar*>&, int);
  void generateCode(XStr&, XStr&, Entry *);
  void generateTrace();
  AtomicConstruct(const char *code, const char *trace_name)
    : SdagConstruct(SATOMIC, NULL, 0, 0, 0, 0, 0, 0)
  {
    char *tmp = strdup(code);
    RemoveSdagComments(tmp);
    text = new XStr(tmp);
    free(tmp);

    if (trace_name)
    {
      tmp = strdup(trace_name);
      tmp[strlen(tmp)-1]=0;
      traceName = new XStr(tmp+1);
      free(tmp);
    }
  }
};

}

#endif
