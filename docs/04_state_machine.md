# Finite State Machine of the Lexical Analyzer

The lexical analyzer (`lexer.c`) is implemented as a deterministic finite automaton that reads the input character by character. It consists of a main automaton and three sub-automata (numbers, strings, comments); the main automaton selects the sub-automaton according to the first character of the lexeme. The diagrams use character classes. A double circle is an accepting state, and an asterisk after its name means that the token is complete only after one extra character has been read, which is returned to the input (`preloadedChar`). Edges that are not drawn lead to the error state `ERR`. All diagrams are placed at the end of this chapter (Figs. \ref{fig:fsa-hlavni}--\ref{fig:fsa-komentare}).

## Main automaton

The main automaton (Fig. \ref{fig:fsa-hlavni}) branches from the state `START` according to the class of the first character. An identifier is a loop over the state `ID`. The state after a leading `_` is non-accepting, so a lone underscore ends in `ERR`. Keywords are not distinguished in the automaton; they are recognized after the lexeme has been read by looking it up in a table. The operators `>`, `<`, `=`, `!` and `|` have an accepting state and move to a two-character token after a following `=` (or `|`), while `&` requires a second `&`. The `@main` annotation is a chain of states, one per character. A line break produces the token `NL`, and the spaces that follow are counted into the token `INDENT`.

## Numbers

The sub-automaton (Fig. \ref{fig:fsa-cisla}) has branches for the integer part, the fractional part and the exponent. The states `0_`, `INT_`, `DEC_` and `EXP_` are non-accepting, so an underscore must be followed by a digit (by a zero after a leading zero). A digit is required after the decimal point, the suffix `d`/`D` leads to its own accepting state, and a letter directly after a finished literal is an error. The resulting tokens are `TK_NUMBER`, `TK_DECIMAL` or `TK_DOT`.

## Strings

After the opening quote, the sub-automaton (Fig. \ref{fig:fsa-retezce}) distinguishes three branches: the empty string, the multi-line string (`"""`) and the single-line string. The single-line branch has the state `ESC` for escape sequences, and the multi-line branch counts consecutive quotes with the states `ML"` and `ML""`.

\newpage

## Comments

After `/`, the sub-automaton (Fig. \ref{fig:fsa-komentare}) branches into division, a line comment and a block comment. A line comment (`//`) ends at a line break or at the end of file, and the line break itself is then processed by the main automaton.

A block comment (`/*`) may be nested, which cannot be expressed by a finite number of states alone. The automaton is therefore extended with two variables:

- **$d$ (nesting depth)** is an integer counter. It is set to 1 when the comment is opened by `/*`. Each nested `/*` increments it (transition `BLOCK/` $\to$ `BLOCK` on `*`), and each `*/` decrements it as long as $d>1$ (transition `BLOCK*` $\to$ `BLOCK` on `/`). The comment is closed by a `*/` read while $d=1$.
- **$nl$ (line-break flag)** is a boolean. It is set to 0 when the comment is opened and to 1 as soon as a line break is read anywhere inside the comment, including nested parts.

The states `BLOCK/` and `BLOCK*` remember the previous character: after `/`, a following `*` opens a nested comment, and after `*`, a following `/` closes one. Any other character returns the automaton to `BLOCK`. A block comment must be closed before the end of file; otherwise the automaton ends in `ERR`.

After the closing `*/` the value of $nl$ decides how the comment is interpreted. If $nl=0$, the comment is equivalent to a single whitespace character (`SPACE`) and merely separates tokens. If $nl=1$, it is equivalent to a line break: the state `LF?` accepts only whitespace until a line break, end of file or the start of another comment, and any other character is an error. In both cases the character that follows is returned to the input.

```{=latex}
\tikzset{
  fa/.style={->, >=Stealth, semithick, auto, initial text=, font=\small,
             every state/.style={minimum size=1.15cm, inner sep=1pt, font=\scriptsize}},
  ref/.style={draw, rounded corners=2pt, minimum height=0.8cm, font=\small, fill=black!5},
  lbl/.style={font=\scriptsize, inner sep=1.5pt},
  note/.style={font=\scriptsize, text=red!70!black, anchor=west},
}

\begin{figure}[p]
\centering
\resizebox{\linewidth}{!}{%
\begin{tikzpicture}[fa]
  \node[state, initial, initial where=above] (S) at (0,-0) {START};
  \path (S) edge[loop right] node[lbl] {\textvisiblespace, ws} (S);

  % --- EOF
  \node[state, accepting] (EOF) at (-4.2,1.7) {EOF};
  \path (S) edge node[lbl, above] {EOF} (EOF);

  % --- jednoznakove tokeny
  \node[state, accepting] (ONE) at (3.8,1.8) {1\,char};
  \path (S) edge node[lbl, below, pos=.8] {$+\;-\;*\;(\;)\;:\;;\;,$} (ONE);

  % --- identifikator
  \node[state] (US) at (7.2,-1) {\_};
  \node[state, accepting] (ID) at (7.2,-3) {ID$^{*}$};
  \path (S) edge[bend left=10] node[lbl, pos=.6, below left] {\texttt{[a-zA-Z]}} (ID)
        (S) edge[bend left=14] node[lbl, pos=.5, above right] {\texttt{\_}} (US)
        (US) edge node[lbl, right] {\texttt{[a-zA-Z0-9\_]}} (ID)
        (ID) edge[loop right] node[lbl] {\texttt{[a-zA-Z0-9\_]}} (ID);
  \node[note, anchor=south] at (7.2,0.1) {lone \_ $\to$ ERR};

  % --- cisla, retezce
  \node[ref] (NUM) at (-2.4,3.8) {numbers (Fig.~\ref{fig:fsa-cisla})};
  \node[ref] (STR) at (-0.5,5.3) {strings (Fig.~\ref{fig:fsa-retezce})};
  \path (S) edge node[lbl, right] {\texttt{[0-9]}, \texttt{.}} (NUM)
        (S) edge node[lbl, left] {\texttt{"}} (STR);

  % --- komentare / deleni
  \node[state, accepting] (DIV) at (2.8,3.7) {\texttt{/}$^{*}$};
  \node[ref] (COM) at (3.2,5.4) {comments (Fig.~\ref{fig:fsa-komentare})};
  \path (S) edge[bend right=8] node[lbl, pos=.55, above left] {\texttt{/}} (DIV)
        (DIV) edge node[lbl, right] {\texttt{/}, \texttt{*}} (COM);

  % --- operatory (levy sloupec = prvni znak, pravy = dvouznakovy token)
  \node[state, accepting] (GT) at (5.2,-5.6) {\texttt{>}$^{*}$};
  \node[state, accepting] (GE) at (5.2,-7.8) {\texttt{>=}};
  \node[state, accepting] (LT) at (3.7,-5.6) {\texttt{<}$^{*}$};
  \node[state, accepting] (LE) at (3.7,-7.8) {\texttt{<=}};
  \node[state, accepting] (AS) at (2.2,-5.6) {\texttt{=}$^{*}$};
  \node[state, accepting] (EQ) at (2.2,-7.8) {\texttt{==}};
  \node[state, accepting] (NT) at (0.7,-5.6) {\texttt{!}$^{*}$};
  \node[state, accepting] (NE) at (0.7,-7.8) {\texttt{!=}};
  \node[state, accepting] (PI) at (-0.8,-5.6) {\texttt{|}$^{*}$};
  \node[state, accepting] (OR) at (-0.8,-7.8) {\texttt{||}};
  \node[state] (AM) at (-2.3,-5.6) {\texttt{\&}};
  \node[state, accepting] (AND) at (-2.3,-7.8) {\texttt{\&\&}};
  \path (S) edge[bend left=6] node[lbl, pos=.85, right] {\texttt{>}} (GT)
        (S) edge[bend left=3] node[lbl, pos=.85, right] {\texttt{<}} (LT)
        (S) edge node[lbl, pos=.85, right] {\texttt{=}} (AS)
        (S) edge[bend right=3] node[lbl, pos=.85, left] {\texttt{!}} (NT)
        (S) edge[bend right=6] node[lbl, pos=.85, left] {\texttt{|}} (PI)
        (S) edge[bend right=9] node[lbl, pos=.85, left] {\texttt{\&}} (AM)
        (GT) edge node[lbl] {\texttt{=}} (GE)
        (LT) edge node[lbl] {\texttt{=}} (LE)
        (AS) edge node[lbl] {\texttt{=}} (EQ)
        (NT) edge node[lbl] {\texttt{=}} (NE)
        (PI) edge node[lbl] {\texttt{|}} (OR)
        (AM) edge node[lbl] {\texttt{\&}} (AND);
  \node[note] at (-3.0,-9.0) {other $\to$ ERR};

  % --- @main
  \node[state] (A1) at (-6.6,0.6) {\texttt{@}};
  \node[state] (A2) at (-6.6,2.5) {\texttt{@m}};
  \node[state] (A3) at (-6.6,4.4) {\texttt{@ma}};
  \node[state] (A4) at (-6.6,6.3) {\texttt{@mai}};
  \node[state, accepting] (AMN) at (-6.6,8.2) {\texttt{@main}$^{*}$};
  \path (S) edge[bend right=10] node[lbl, pos=.6, above] {\texttt{@}} (A1)
        (A1) edge node[lbl, above] {m} (A2)
        (A2) edge node[lbl, above] {a} (A3)
        (A3) edge node[lbl, below] {i} (A4)
        (A4) edge node[lbl, below left] {n} (AMN);
  \node[note, anchor=north, align=left] at (-3.8,8.2) {\texttt{@main} must not be followed\\by a letter, digit or \_};

  % --- novy radek a odsazeni
  \node[state, accepting] (NL) at (-4.5,-1.5) {NL};
  \node[state, accepting] (IND) at (-4.9,-3.6) {INDENT$^{*}$};
  \path (S) edge node[lbl, below, pos=.5] {\texttt{\textbackslash n}} (NL)
        (NL) edge node[lbl, below] {\textvisiblespace} (IND)
        (IND) edge[loop right] node[lbl] {\textvisiblespace\ ($n{+}{+}$)} (IND);
  \node[note, anchor=north, align=center] at (-4.9,-4.6) {tab in indentation\\$\to$ ERR};
\end{tikzpicture}}
\caption{Main finite automaton of the lexical analyzer}
\label{fig:fsa-hlavni}
\end{figure}

\begin{figure}[p]
\centering
\resizebox{\linewidth}{!}{%
\begin{tikzpicture}[fa]
  \node[state, initial, initial where=above] (S) at (0,-0) {START};
  \node[state, accepting] (Z) at (3,-2.52) {0$^{*}$};
  \node[state] (ZU) at (6.2,-2.52) {0\_};
  \node[state, accepting] (I) at (-3,-2.52) {INT$^{*}$};
  \node[state] (IU) at (-6,-2.52) {INT\_};
  \node[state] (DT) at (0,-5.4) {\texttt{.}};
  \node[state] (P0) at (-3.2,-5.4) {\texttt{.}};
  \node[state, accepting] (TD) at (-6,-5.4) {DOT$^{*}$};
  \node[state, accepting] (F) at (0,-7.92) {DEC$^{*}$};
  \node[state] (FU) at (3.2,-7.92) {DEC\_};
  \node[state] (E) at (0,-10.8) {E};
  \node[state] (ES) at (3.2,-10.8) {E$\pm$};
  \node[state, accepting] (ED) at (0,-13.68) {EXP$^{*}$};
  \node[state] (EU) at (3.2,-13.68) {EXP\_};
  \node[state, accepting] (D) at (0,-16.56) {DEC\,d};

  % --- zacatek
  \path (S) edge[bend left=10] node[lbl, above right] {\texttt{0}} (Z)
        (S) edge[bend right=10] node[lbl, above left] {\texttt{[1-9]}} (I)
        (S) edge[bend right=12] node[lbl, pos=.35, above left] {\texttt{.}} (P0)
        (P0) edge node[lbl, above] {other} (TD)
        (P0) edge[bend left=12] node[lbl, pos=.35, above] {\texttt{[0-9]}} (F);

  % --- cela cast
  \path (Z) edge[loop above] node[lbl] {\texttt{0}} (Z)
        (Z) edge[bend left=15] node[lbl, above] {\texttt{\_}} (ZU)
        (ZU) edge[bend left=15] node[lbl, below] {\texttt{0}} (Z)
        (ZU) edge[loop above] node[lbl] {\texttt{\_}} (ZU)
        (I) edge[loop above] node[lbl] {\texttt{[0-9]}} (I)
        (I) edge[bend left=15] node[lbl, above] {\texttt{\_}} (IU)
        (IU) edge[bend left=15] node[lbl, below] {\texttt{[0-9]}} (I)
        (IU) edge[loop above] node[lbl] {\texttt{\_}} (IU)
        (Z) edge node[lbl, below right] {\texttt{.}} (DT)
        (I) edge node[lbl, below left, pos=.45] {\texttt{.}} (DT);

  % --- desetinna cast
  \path (DT) edge node[lbl, right] {\texttt{[0-9]}} (F)
        (F) edge[loop left] node[lbl] {\texttt{[0-9]}} (F)
        (F) edge[bend left=15] node[lbl, above] {\texttt{\_}} (FU)
        (FU) edge[bend left=15] node[lbl, below] {\texttt{[0-9]}} (F)
        (FU) edge[loop right] node[lbl] {\texttt{\_}} (FU);

  % --- exponent
  \path (Z) edge[bend left=30] node[lbl, right] {\texttt{e}, \texttt{E}} (E)
        (I) edge[bend right=12] node[lbl, left, pos=.15] {\texttt{e}, \texttt{E}} (E)
        (F) edge node[lbl, right] {\texttt{e}, \texttt{E}} (E)
        (E) edge node[lbl, above] {\texttt{+}, \texttt{-}} (ES)
        (E) edge node[lbl, right] {\texttt{[0-9]}} (ED)
        (ES) edge node[lbl, below right] {\texttt{[0-9]}} (ED)
        (ED) edge[loop left] node[lbl] {\texttt{[0-9]}} (ED)
        (ED) edge[bend left=15] node[lbl, above] {\texttt{\_}} (EU)
        (EU) edge[bend left=15] node[lbl, below] {\texttt{[0-9]}} (ED)
        (EU) edge[loop right] node[lbl] {\texttt{\_}} (EU);

  % --- pripona d/D
  \path (F) edge[bend right=40] node[lbl, left] {\texttt{d}, \texttt{D}} (D)
        (ED) edge node[lbl, right] {\texttt{d}, \texttt{D}} (D);
\end{tikzpicture}}
\caption{Sub-automaton for numeric literals}
\label{fig:fsa-cisla}
\end{figure}

\begin{figure}[p]
\centering
\resizebox{\linewidth}{!}{%
\begin{tikzpicture}[fa]
  \node[state, initial, initial where=above] (S) at (0,-0) {START};
  \node[state] (Q1) at (0,-2.4) {\texttt{"}};
  \node[state] (Q2) at (-3.5,-4.4) {\texttt{""}};
  \node[state, accepting] (EMP) at (-6.5,-4.4) {\texttt{""}$^{*}$};

  % --- jednoradkovy
  \node[state] (STR) at (3.2,-5.6) {STR};
  \node[state] (ESC) at (0,-5.6) {ESC};
  \node[state, accepting] (SEND) at (6.5,-5.6) {STRING};
  \path (S) edge node[lbl, right] {\texttt{"}} (Q1)
        (Q1) edge[bend left=20] node[lbl, above right] {other} (STR)
        (Q1) edge node[lbl, right] {\texttt{\textbackslash}} (ESC)
        (Q1) edge node[lbl, above] {\texttt{"}} (Q2)
        (STR) edge[loop below] node[lbl, align=left] {other than \texttt{"} \texttt{\textbackslash}\\LF, EOF} (STR)
        (STR) edge[bend left=15] node[lbl, below] {\texttt{\textbackslash}} (ESC)
        (ESC) edge[bend left=15] node[lbl, above] {\texttt{\textbackslash\textbackslash\ " n t r}} (STR)
        (STR) edge node[lbl, above] {\texttt{"}} (SEND);

  % --- viceradkovy
  \node[state] (ML) at (-3.5,-8) {ML};
  \node[state] (M1) at (-3.5,-11.2) {ML\texttt{"}};
  \node[state] (M2) at (-3.5,-14.4) {ML\texttt{""}};
  \node[state, accepting] (MEND) at (-6.5,-14.4) {STRING};
  \path (Q2) edge node[lbl, right] {\texttt{"}} (ML)
        (Q2) edge node[lbl, above] {other} (EMP)
        (ML) edge[loop right] node[lbl] {other than \texttt{"} (incl. LF)} (ML)
        (ML) edge[bend left=15] node[lbl, right] {\texttt{"}} (M1)
        (M1) edge[bend left=15] node[lbl, left] {other} (ML)
        (M1) edge node[lbl, right] {\texttt{"}} (M2)
        (M2) edge[bend left=25] node[lbl, left] {other} (ML)
        (M2) edge node[lbl, above] {\texttt{"}} (MEND);

  \node[note] at (2.0,-10.3) {other character after \texttt{\textbackslash} $\to$ ERR};
  \node[note] at (2.0,-11.0) {EOF inside a multi-line string $\to$ ERR};
\end{tikzpicture}}
\caption{Sub-automaton for string literals}
\label{fig:fsa-retezce}
\end{figure}

\begin{figure}[p]
\centering
\resizebox{\linewidth}{!}{%
\begin{tikzpicture}[fa]
  \node[state, initial, initial where=above] (S) at (0,0) {START};
  \node[state, accepting] (DIV) at (0,-2.4) {\texttt{/}$^{*}$};

  \node[state] (LC) at (5,-5) {\texttt{//}};
  \node[state, accepting] (LCE) at (5,-8) {LF$^{*}$};
  \path (S) edge node[lbl, right] {\texttt{/}} (DIV)
        (DIV) edge[bend left=15] node[lbl, above right] {\texttt{/}} (LC)
        (LC) edge[loop right] node[lbl] {other than LF, EOF} (LC)
        (LC) edge node[lbl, right] {LF, EOF} (LCE);

  \node[state] (BL) at (-6.5,-5) {BLOCK\texttt{/}};
  \node[state] (B) at (-2.5,-5) {BLOCK};
  \node[state] (BS) at (-2.5,-8) {BLOCK\texttt{*}};
  \node[state] (AF) at (-2.5,-11) {\texttt{*/}};
  \node[state, accepting] (WS) at (-5.5,-13.8) {SPACE$^{*}$};
  \node[state] (AN) at (0.5,-13.8) {LF?};
  \node[state, accepting] (NLE) at (0.5,-16.8) {NL$^{*}$};
  \path (DIV) edge node[lbl, left, align=right] {\texttt{*}\ ($d{:=}1$,\\$nl{:=}0$)} (B)
        (B) edge[loop right] node[lbl, align=left] {other\\LF ($nl{:=}1$)} (B)
        (B) edge[bend left=22] node[lbl, below] {\texttt{/}} (BL)
        (BL) edge[bend left=22] node[lbl, above, align=center] {\texttt{*} ($d{+}{+}$)\\other} (B)
        (BL) edge[loop left] node[lbl] {\texttt{/}} (BL)
        (B) edge[bend left=22] node[lbl, right] {\texttt{*}} (BS)
        (BS) edge[bend left=22] node[lbl, left, align=right] {other\\\texttt{/}, $d{>}1$ ($d{-}{-}$)} (B)
        (BS) edge[loop right] node[lbl] {\texttt{*}} (BS)
        (BS) edge node[lbl, right] {\texttt{/}, $d{=}1$} (AF)
        (AF) edge node[lbl, left] {$nl{=}0$} (WS)
        (AF) edge node[lbl, right, pos=.55] {$nl{=}1$} (AN)
        (AN) edge[loop right] node[lbl] {\textvisiblespace, ws} (AN)
        (AN) edge node[lbl, right] {LF, EOF, \texttt{//}, \texttt{/*}} (NLE);
  \node[note, align=left] at (1.2,-10.2) {EOF inside a block comment $\to$ ERR};
  \node[note, align=left] at (1.2,-11.0) {$nl{=}1$ and, after \texttt{*/} (and whitespace),\\a character other than LF, EOF or a comment $\to$ ERR};
\end{tikzpicture}}
\caption{Sub-automaton for comments}
\label{fig:fsa-komentare}
\end{figure}
```

\newpage
