
# Syntax analysis

It consists of two main parts: Recursive descent parsing by applying LL grammar on it, and Precedent SA.


# LL grammar
This is grammar for whole compiler. In terms of implementation, it works such that each rule corresponds to one function. Because of mandatory requirement of precedent SA in IFJ project, this grammar is used only partially - the block of expression rules is not part of the implementation (expression are processed in the precedence SA) That resulted into modification of rules which remained and are in relation with expression non-terminals rules. Some rules needed higher look-ahead, therefore implementation isn't using clean LL(1) grammar, but LL(2) as well.

```
// ---- program ----
Program     -> Prolog FunDefs TK_EOF
Prolog      -> TK_KW_IMPORT TK_IDENT TK_DOT TK_MULTIPLY TK_NEWLINE
// check TK_IDENT == "ifj26"

FunDefs     -> FunDef FunDefsTail
FunDefsTail -> FunDef FunDefsTail | ε

FunDef      -> MainAnnot TK_KW_DEF TK_IDENT TK_LPAREN Params TK_RPAREN
                OptType TK_ASSIGN FunBody
MainAnnot   -> TK_ATMAIN MainNL | ε
MainNL      -> TK_NEWLINE | ε
FunBody     -> TK_NEWLINE TK_INDENT Block TK_DEDENT
             | RValue TK_NEWLINE

Params      -> Param ParamsTail | ε
ParamsTail  -> TK_COMMA Param ParamsTail | ε
Param       -> TK_IDENT TK_COLON Type
OptType     -> TK_COLON Type | ε
Type        -> TK_KW_INT | TK_KW_DOUBLE | TK_KW_STRING | TK_KW_UNIT

// ---- statements ----
Block       -> Line LinesTail
LinesTail   -> Line LinesTail | ε
Line        -> Simple SimpleTail TK_NEWLINE
             | If
             | While
SimpleTail  -> TK_SEMICOLON Simple SimpleTail | ε

Simple      -> VarDecl
             | Return
             | TK_IDENT IdentTail
             | PrefixNI ExpRest                  // expression statement not starting with ident
IdentTail   -> TK_ASSIGN RValue                  // assignment
             | TK_LPAREN Args TK_RPAREN          // call statement
             | ExpRest                           // expression statement starting with ident

VarDecl     -> VarKw TK_IDENT OptType TK_ASSIGN RValue
VarKw       -> TK_KW_VAL | TK_KW_VAR
Return      -> TK_KW_RETURN ReturnTail
ReturnTail  -> RValue | ε

RValue      -> TK_IDENT RValueTail | PrefixNI ExpRest      // "expression or call"
RValueTail  -> TK_LPAREN Args TK_RPAREN | ExpRest

Args        -> Term ArgsTail | ε
ArgsTail    -> TK_COMMA Term ArgsTail | ε
Term        -> TK_IDENT | Lit | TK_LPAREN TK_RPAREN

If          -> TK_KW_IF Exp TK_KW_THEN TK_NEWLINE TK_INDENT Block TK_DEDENT
               TK_KW_ELSE TK_NEWLINE TK_INDENT Block TK_DEDENT
While       -> TK_KW_WHILE Exp TK_KW_DO TK_NEWLINE TK_INDENT Block TK_DEDENT


// ---- expressions ----
Exp         -> Rel EqTail
EqTail      -> EqOp Rel | ε
EqOp        -> TK_EQUAL | TK_NOT_EQUAL
Rel         -> Sum RelTail
RelTail     -> RelOp Sum | ε
RelOp       -> TK_SMALLER | TK_SEQUAL | TK_BIGGER | TK_BEQUAL
Sum         -> Product SumTail
SumTail     -> AddOp Product SumTail | ε
AddOp       -> TK_PLUS | TK_MINUS
Product     -> Prefix ProductTail
ProductTail -> MulOp Prefix ProductTail | ε
MulOp       -> TK_MULTIPLY | TK_DIVIDE
Prefix      -> TK_MINUS Prefix | Primary
Primary     -> TK_IDENT | PrimaryNI
PrefixNI    -> TK_MINUS Prefix | PrimaryNI
PrimaryNI   -> Lit | TK_LPAREN ParenTail
ParenTail   -> TK_RPAREN | Exp TK_RPAREN          // "()" is the Unit literal
Lit         -> TK_NUMBER | TK_DECIMAL | TK_STRING
ExpRest     -> ProductTail SumTail RelTail EqTail // rest of an Exp after its first operand
```

# Precedence table
Created based on the table structure template from the 8th lecture.
It serves as a precedence-based syntactic analyzer together with an implemented stack, into which we push elements from the input whenever this table allows us to do so.
This table makes decisions based on the input character/token and the top terminal (T) on the stack, and determines what will happen to the current stack:

[T][x]  | input x          |
|:-----:| :--------------: |
|'='    | Shift x on stack |
|'<'    | Set brake and shift x on stack |
|'>'    | Reduce a combination of characters to a non-terminal|
|'E'    | Error |


\begin{table}[H]
\centering
\begin{tabular}{|c|c|c|c|c|c|c|c|c|c|c|c|}
\hline
st \textbackslash{} IN & + -            & * /            & Un          & REL            & EQ             & (           & )              & i           & \$             & AND            & OR             \\ \hline
+ -                    & \textgreater{} & \textless{}    & \textless{} & \textgreater{} & \textgreater{} & \textless{} & \textgreater{} & \textless{} & \textgreater{} & \textgreater{} & \textgreater{} \\ \hline
{}* /                  & \textgreater{} & \textgreater{} & \textless{} & \textgreater{} & \textgreater{} & \textless{} & \textgreater{} & \textless{} & \textgreater{} & \textgreater{} & \textgreater{} \\ \hline
Unary                  & \textgreater{} & \textgreater{} & \textless{} & \textgreater{} & \textgreater{} & \textless{} & \textgreater{} & \textless{} & \textgreater{} & \textgreater{} & \textgreater{} \\ \hline
REL                    & \textless{}    & \textless{}    & \textless{} & E              & \textgreater{} & \textless{} & \textgreater{} & \textless{} & \textgreater{} & \textgreater{} & \textgreater{} \\ \hline
EQ                     & \textless{}    & \textless{}    & \textless{} & \textless{}    & E              & \textless{} & \textgreater{} & \textless{} & \textgreater{} & \textgreater{} & \textgreater{} \\ \hline
(                      & \textless{}    & \textless{}    & \textless{} & \textless{}    & \textless{}    & \textless{} & =              & \textless{} & E              & \textless{}    & \textless{}    \\ \hline
)                      & \textgreater{} & \textgreater{} & E           & \textgreater{} & \textgreater{} & E           & \textgreater{} & E           & \textgreater{} & \textgreater{} & \textgreater{} \\ \hline
i                      & \textgreater{} & \textgreater{} & E           & \textgreater{} & \textgreater{} & E           & \textgreater{} & E           & \textgreater{} & \textgreater{} & \textgreater{} \\ \hline
\$                     & \textless{}    & \textless{}    & \textless{} & \textless{}    & \textless{}    & \textless{} & E              & \textless{} & E              & \textless{}    & \textless{}    \\ \hline
AND                    & \textless{}    & \textless{}    & \textless{} & \textless{}    & \textless{}    & \textless{} & \textgreater{} & \textless{} & \textgreater{} & \textgreater{} & \textgreater{} \\ \hline
OR                     & \textless{}    & \textless{}    & \textless{} & \textless{}    & \textless{}    & \textless{} & \textgreater{} & \textless{} & \textgreater{} & \textless{}    & \textgreater{} \\ \hline
\end{tabular}
\end{table}



**Legend:**

$+ - * /$ -- operators

Un,Unary = Unary operator ($!x, -x$)

REL = relational operators ($<$,$>$,$<=$,$>=$)

EQ = operators ($==$,$!=$)

i = operand (variable, literal)

$ = ending element on input/ starting element on stack

AND = operator $\&\&$

OR = operator $||$

<!-- | st \ IN 	| + - 	| * / 	| Un 	| REL 	| EQ 	| ( 	| ) 	| i 	| $ 	| AND 	| OR 	|
|:-------:	|:---:	|:---:	|:--:	|:---:	|:--:	|:-:	|:-:	|:-:	|:-:	|:---:	|:--:	|
|   + -   	|  >  	|  <  	|  < 	|  >  	|  > 	| < 	| > 	| < 	| > 	|  >  	|  > 	|
|   * /   	|  >  	|  >  	|  < 	|  >  	|  > 	| < 	| > 	| < 	| > 	|  >  	|  > 	|
|  Unary  	|  >  	|  >  	|  < 	|  >  	|  > 	| < 	| > 	| < 	| > 	|  >  	|  > 	|
|   REL   	|  <  	|  <  	|  < 	|  E  	|  > 	| < 	| > 	| < 	| > 	|  >  	|  > 	|
|    EQ   	|  <  	|  <  	|  < 	|  <  	|  E 	| < 	| > 	| < 	| > 	|  >  	|  > 	|
|    (    	|  <  	|  <  	|  < 	|  <  	|  < 	| < 	| = 	| < 	| E 	|  <  	|  < 	|
|    )    	|  >  	|  >  	|  E 	|  >  	|  > 	| E 	| > 	| E 	| > 	|  >  	|  > 	|
|    i    	|  >  	|  >  	|  E 	|  >  	|  > 	| E 	| > 	| E 	| > 	|  >  	|  > 	|
|    $    	|  <  	|  <  	|  < 	|  <  	|  < 	| < 	| E 	| < 	| E 	|  <  	|  < 	|
|   AND   	|  <  	|  <  	|  < 	|  <  	|  < 	| < 	| > 	| < 	| > 	|  >  	|  > 	|
|    OR   	|  <  	|  <  	|  < 	|  <  	|  < 	| < 	| > 	| < 	| > 	|  <  	|  > 	| -->

\newpage
