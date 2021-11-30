#ifndef _cadical_hpp_INCLUDED
#define _cadical_hpp_INCLUDED

#include <cstdio>
#include <cstdint>
#include <vector>

namespace CaDiCaL {

/*========================================================================*/

// This provides the actual API of the CaDiCaL solver, which is implemented
// in the class 'Solver' below.  Beside its constructor and destructor most
// important is the IPASIR part which you can find between 'BEGIN IPASIR'
// and 'END IPASIR' comments below.  The following '[Example]' below might
// also be a good starting point to understand the API.

/*========================================================================*/

// [Example]
//
// The internal solver state follows the IPASIR API model used in the
// incremental track of the SAT competition.  State transitions are
// triggered by member function calls, declared and described below.
//
// Consider the following code (from 'test/api/example.cpp') of API usage:
//
//   CaDiCaL::Solver * solver = new CaDiCaL::Solver;
//
//   // ------------------------------------------------------------------
//   // Encode Problem and check without assumptions.
//
//   enum { TIE = 1, SHIRT = 2 };
//
//   solver->add (-TIE), solver->add (SHIRT),  solver->add (0);
//   solver->add (TIE),  solver->add (SHIRT),  solver->add (0);
//   solver->add (-TIE), solver->add (-SHIRT), solver->add (0);
//
//   int res = solver->solve ();    // Solve instance.
//   assert (res == 10);            // Check it is 'SATISFIABLE'.
//
//   res = solver->val (TIE);       // Obtain assignment of 'TIE'.
//   assert (res < 0);              // Check 'TIE' assigned to 'false'.
//
//   res = solver->val (SHIRT);     // Obtain assignment of 'SHIRT'.
//   assert (res > 0);              // Check 'SHIRT' assigned to 'true'.
//
//   // ------------------------------------------------------------------
//   // Incrementally solve again under one assumption.
//
//   solver->assume (TIE);          // Now force 'TIE' to true.
//
//   res = solver->solve ();        // Solve again incrementally.
//   assert (res == 20);            // Check it is 'UNSATISFIABLE'.
//
//   res = solver->failed (TIE);    // Check 'TIE' responsible.
//   assert (res);                  // Yes, 'TIE' in core.
//
//   res = solver->failed (SHIRT);  // Check 'SHIRT' responsible.
//   assert (!res);                 // No, 'SHIRT' not in core.
//
//   // ------------------------------------------------------------------
//   // Incrementally solve once more under another assumption.
//
//   solver->assume (-SHIRT);       // Now force 'SHIRT' to false.
//
//   res = solver->solve ();        // Solve again incrementally.
//   assert (res == 20);            // Check it is 'UNSATISFIABLE'.
//
//   res = solver->failed (TIE);    // Check 'TIE' responsible.
//   assert (!res);                 // No, 'TIE' not in core.
//
//   res = solver->failed (-SHIRT); // Check '!SHIRT' responsible.
//   assert (res);                  // Yes, '!SHIRT' in core.
//
//   // ------------------------------------------------------------------
//
//   delete solver;

/*========================================================================*/

// [States and Transitions]
//
// Compared to IPASIR we also use an 'ADDING' state in which the solver
// stays while adding non-zero literals until the clause is completed
// through adding a zero literal.  The additional 'INITIALIZING',
// 'CONFIGURING' and 'DELETING' states are also not part of IPASIR but also
// useful for testing and debugging.
//
// We have the following transitions which are all synchronous except for
// the reentrant 'terminate' call:
//
//                         new
// INITIALIZING --------------------------> CONFIGURING
//
//                    set / trace
//  CONFIGURING --------------------------> CONFIGURING
//
//               add (non zero literal)
//        VALID --------------------------> ADDING
//
//               add (zero literal)
//        VALID --------------------------> UNKNOWN
//
//               assume (non zero literal)
//        READY --------------------------> UNKNOWN
//
//                        solve
//        READY --------------------------> SOLVING
//
//                     (internal)
//      SOLVING --------------------------> READY
//
//                val (non zero literal)
//    SATISFIED --------------------------> SATISFIED
//
//               failed (non zero literal )
//  UNSATISFIED --------------------------> UNSATISFIED
//
//                        delete
//        VALID --------------------------> DELETING
//
// where
//
//        READY = CONFIGURING  | UNKNOWN | SATISFIED | UNSATISFIED
//        VALID = READY        | ADDING
//      INVALID = INITIALIZING | DELETING
//
// The 'SOLVING' state is only visible in different contexts, i.e., from
// another thread or from a signal handler.  It is used to implement
// 'terminate'.  Here is the only asynchronous transition:
//
//               terminate (asynchronously)
//      SOLVING  ------------------------->  UNKNOWN
//
// The important behaviour to remember is that adding or assuming a literal
// (immediately) destroys the satisfying assignment in the 'SATISFIED' state
// and vice versa resets all assumptions in the 'UNSATISFIED' state.  This
// is exactly the behaviour required by the IPASIR interface.
//
// Furthermore, the model can only be queried through 'val' in the
// 'SATISFIED' state, while extracting failed assumptions with 'val' only in
// the 'UNSATISFIED' state.  Solving can only be started in the 'UNKNOWN' or
// 'CONFIGURING' state or after the previous call to 'solve' yielded an
// 'UNKNOWN, 'SATISFIED' or 'UNSATISFIED' state.
//
// All literals have to be valid literals too, i.e., 32-bit integers
// different from 'INT_MIN'.  If any of these requirements is violated the
// solver aborts with an 'API contract violation' message.
//
// HINT: If you do not understand why a contract is violated you can run
// 'mobical' on the failing API call trace.  Point the environment variable
// 'CADICAL_API_TRACE' to the file where you want to save the trace during
// execution of your program linking against the library.  You probably need
// for 'mobical' to use the option '--do-not-enforce-contracts' though to
// force running into the same contract violation.
//
// Additional API calls (like 'freeze' and 'melt') do not change the state
// of the solver and are all described below.

/*========================================================================*/

// States are represented by a bit-set in order to combine them.

enum State
{
  INITIALIZING = 1,             // during initialization (invalid)
  CONFIGURING  = 2,             // configure options (with 'set')
  UNKNOWN      = 4,             // ready to call 'solve'
  ADDING       = 8,             // adding clause literals (zero missing)
  SOLVING      = 16,            // while solving (within 'solve')
  SATISFIED    = 32,            // satisfiable allows 'val'
  UNSATISFIED  = 64,            // unsatisfiable allows 'failed'
  DELETING     = 128,           // during and after deletion (invalid)

  // These combined states are used to check contracts.

  READY   = CONFIGURING  | UNKNOWN | SATISFIED | UNSATISFIED,
  VALID   = READY        | ADDING,
  INVALID = INITIALIZING | DELETING
};

/*------------------------------------------------------------------------*/

// Opaque classes needed in the API and declared in the same namespace.

class Clause;
class File;
struct Internal;
struct External;

/*------------------------------------------------------------------------*/

// Forward declaration of call-back classes. See bottom of this file.

class Learner;
class Terminator;
class ClauseIterator;
class WitnessIterator;
class LearnSource;
class Rater;

/*------------------------------------------------------------------------*/

class Solver {

public:

  // ====== BEGIN IPASIR ===================================================

  // This section implements the corresponding IPASIR functionality.

  Solver ();
  ~Solver ();

  static const char * signature ();     // name of this library

  // Core functionality as in the IPASIR incremental SAT solver interface.
  // (recall 'READY = CONFIGURING | UNKNOWN | SATISFIED | UNSATISFIED').
  // Further note that 'lit' is required to be different from 'INT_MIN' and
  // different from '0' except for 'add'.

  // Add valid literal to clause or zero to terminate clause.
  //
  //   require (VALID)                  // recall 'VALID = READY | ADDING'
  //   if (lit) ensure (ADDING)         // and thus VALID but not READY
  //   if (!lit) ensure (UNKNOWN)       // and thus READY
  //
  void add (int lit);

  // Assume valid non zero literal for next call to 'solve'.  These
  // assumptions are reset after the call to 'solve' as well as after
  // returning from 'simplify' and 'lookahead.
  //
  //   require (READY)
  //   ensure (UNKNOWN)
  //
  void assume (int lit);

  // Try to solve the current formula.  Returns
  //
  //    0 = UNSOLVED     (limit reached or interrupted through 'terminate')
  //   10 = SATISFIABLE
  //   20 = UNSATISFIABLE
  //
  //   require (READY)
  //   ensure (UNKNOWN | SATISFIED | UNSATISFIED)
  //
  // Note, that while in this call the solver actually transitions to state
  // 'SOLVING', which however is only visible from a different context,
  // i.e., from a different thread or from a signal handler.  Only right
  // before returning from this call it goes into a 'READY' state.
  //
  int solve ();

  // Get value (-lit=false, lit=true) of valid non-zero literal.
  //
  //   require (SATISFIED)
  //   ensure (SATISFIED)
  //
  int val (int lit);

  // Determine whether the valid non-zero literal is in the core.
  // Returns 'true' if the literal is in the core and 'false' otherwise.
  // Note that the core does not have to be minimal.
  //
  //   require (UNSATISFIED)
  //   ensure (UNSATISFIED)
  //
  bool failed (int lit);

  // Add call-back which is checked regularly for termination.  There can
  // only be one terminator connected.  If a second (non-zero) one is added
  // the first one is implicitly disconnected.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  void connect_terminator (Terminator * terminator);
  void disconnect_terminator ();

  // Add call-back which allows to export learned clauses.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  void connect_learner (Learner * learner);
  void disconnect_learner ();

  void connect_learn_source (LearnSource * learnSource);
  void disconnect_learn_source ();

  void connect_rater (Rater * rater);
  void disconnect_rater ();

  struct Statistics {
    int64_t conflicts;    // generated conflicts in 'propagate'
    int64_t decisions;    // number of decisions in 'decide'
    int64_t propagations; // total # propagations
    int64_t restarts;     // total # restarts
  };
  Statistics get_stats ();

  // ====== END IPASIR =====================================================

  //------------------------------------------------------------------------
  // This function determines a good splitting literal.  The result can be
  // zero if the formula is proven to be satisfiable or unsatisfiable.  This
  // can then be checked by 'state ()'.  If the formula is empty and
  // the function is not able to determine satisfiability also zero is
  // returned but the state remains unknown.
  //
  //   require (READY)
  //   ensure (UNKNOWN|SATISFIED|UNSATISFIED)
  //
  int lookahead(void);

  struct CubesWithStatus {
    int status;
    std::vector<std::vector<int>> cubes;
  };

  CubesWithStatus generate_cubes(int, int min_depth = 0);

  void reset_assumptions();

  // Return the current state of the solver as defined above.
  //
  const State & state () const { return _state; }

  // Similar to 'state ()' but using the staddard competition exit codes of
  // '10' for 'SATISFIABLE', '20' for 'UNSATISFIABLE' and '0' otherwise.
  //
  int status () const {
         if (_state == SATISFIED)   return 10;
    else if (_state == UNSATISFIED) return 20;
    else                            return 0;
  }

  /*----------------------------------------------------------------------*/

  static const char * version ();    // return version string

  /*----------------------------------------------------------------------*/
  // Copy 'this' into a fresh 'other'.  The copy procedure is not a deep
  // clone, but only copies irredundant clauses and units.  It also makes
  // sure that witness reconstruction works with the copy as with the
  // original formula such that both solvers have the same models.
  // Assumptions are not copied.  Options however are copied as well as
  // flags which remember the current state of variables in preprocessing.
  //
  //   require (READY)          // for 'this'
  //   ensure (READY)           // for 'this'
  //
  //   other.require (CONFIGURING)
  //   other.ensure (CONFIGURING | UNKNOWN)
  //
  void copy (Solver & other) const;

  /*----------------------------------------------------------------------*/
  // Variables are usually added and initialized implicitly whenever a
  // literal is used as an argument except for the functions 'val', 'fixed',
  // 'failed' and 'frozen'.  However, the library internally keeps a maximum
  // variable index, which can be queried.
  //
  //   require (VALID | SOLVING)
  //   ensure (VALID | SOLVING)
  //
  int vars ();

  // Increase the maximum variable index explicitly.  This function makes
  // sure that at least 'min_max_var' variables are initialized.  Since it
  // might need to reallocate tables, it destroys a satisfying assignment
  // and has the same state transition and conditions as 'assume' etc.
  //
  //   require (READY)
  //   ensure (UNKNOWN)
  //
  void reserve (int min_max_var);

#ifndef NTRACING
  //------------------------------------------------------------------------
  // This function can be used to write API calls to a file.  The same
  // format is used which 'mobical' can read, execute and also shrink
  // through delta debugging.
  //
  // Tracing API calls can also be achieved by using the environment
  // variable 'CADICAL_API_TRACE'.  That alternative is useful if you do not
  // want to change the source code using the solver, e.g., if you only have
  // a binary with the solver linked in.  However, that method only allows
  // to trace one solver instance, while with the following function API
  // tracing can be enabled for different solver instances individually.
  //
  // The solver will flush the file after every trace API call but does not
  // close it during deletion. It remains owned by the user of the library.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  void trace_api_calls (FILE * file);
#endif

  //------------------------------------------------------------------------
  // Option handling.

  // Determine whether 'name' is a valid option name.
  //
  static bool is_valid_option (const char * name);

  // Determine whether 'name' enables a specific preprocessing technique.
  //
  static bool is_preprocessing_option (const char * name);

  // Determine whether 'arg' is a valid long option of the form '--<name>',
  // '--<name>=<val>' or '--no-<name>' similar to 'set_long_option' below.
  // Legal values are 'true', 'false', or '[-]<mantissa>[e<exponent>]'.

  static bool is_valid_long_option (const char * arg);

  // Get the current value of the option 'name'.  If 'name' is invalid then
  // zero is returned.  Here '--...' arguments as invalid options.
  //
  int get (const char * name);

  // Set the default verbose message prefix (default "c ").
  //
  void prefix (const char * verbose_message_prefix);

  // Explicit version of setting an option.  If the option '<name>' exists
  // and '<val>' can be parsed then 'true' is returned.  If the option value
  // is out of range the actual value is computed as the closest (minimum or
  // maximum) value possible, but still 'true' is returned.
  //
  //   require (CONFIGURING)
  //   ensure (CONFIGURING)
  //
  // Thus options can only bet set right after initialization.
  //
  bool set (const char * name, int val);

  // This function accepts options in command line syntax:
  //
  //   '--<name>=<val>', '--<name>', or '--no-<name>'
  //
  // It actually calls the previous 'set' function after parsing 'arg'.  The
  // same values are expected as for 'is_valid_long_option' above and as
  // with 'set' any value outside of the range of legal values for a
  // particular option are set to either the minimum or maximum depending on
  // which side of the valid interval they lie.
  //
  //   require (CONFIGURING)
  //   ensure (CONFIGURING)
  //
  bool set_long_option (const char * arg);

  // Determine whether 'name' is a valid configuration.
  //
  static bool is_valid_configuration (const char *);

  // Overwrite (some) options with the forced values of the configuration.
  // The result is 'true' iff the 'name' is a valid configuration.
  //
  //   require (CONFIGURING)
  //   ensure (CONFIGURING)
  //
  bool configure (const char *);

  // Increase preprocessing and inprocessing limits by '10^<val>'.  Values
  // below '0' are ignored and values above '9' are reduced to '9'.
  //
  //   require (READY)
  //   ensure (READY)
  //
  void optimize (int val);

  // Specify search limits, where currently 'name' can be "conflicts",
  // "decisions", "preprocessing", or "localsearch".  The first two limits
  // are unbounded by default.  Thus using a negative limit for conflicts or
  // decisions switches back to the default of unlimited search (for that
  // particular limit).  The preprocessing limit determines the number of
  // preprocessing rounds, which is zero by default.  Similarly, the local
  // search limit determines the number of local search rounds (also zero by
  // default).  As with 'set', the return value denotes whether the limit
  // 'name' is valid.  These limits are only valid for the next 'solve' or
  // 'simplify' call and reset to their default after 'solve' returns (as
  // well as overwritten and reset during calls to 'simplify' and
  // 'lookahead').  We actually also have an internal "terminate" limit
  // which however should only be used for testing and debugging.
  //
  //   require (READY)
  //   ensure (READY)
  //
  bool limit (const char * arg, int val);
  bool is_valid_limit (const char * arg);

  // The number of currently active variables and clauses can be queried by
  // these functions.  Variables become active if a clause is added with it.
  // They become inactive if they are eliminated or fixed at the root level
  // Clauses become inactive if they are satisfied, subsumed, eliminated.
  // Redundant clauses are reduced regularly and thus the 'redundant'
  // function is less useful.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  int active () const;          // Number of active variables.
  int64_t redundant () const;   // Number of active redundant clauses.
  int64_t irredundant () const; // Number of active irredundant clauses.

  //------------------------------------------------------------------------
  // This function executes the given number of preprocessing rounds. It is
  // similar to 'solve' with 'limits ("preprocessing", rounds)' except that
  // no CDCL nor local search, nor lucky phases are executed.  The result
  // values are also the same: 0=unknown, 10=satisfiable, 20=unsatisfiable.
  // As 'solve' it resets current assumptions and limits before returning.
  // The numbers of rounds should not be negative.  If the number of rounds
  // is zero only clauses are restored (if necessary) and top level unit
  // propagation is performed, which both take some time.
  //
  //   require (READY)
  //   ensure (UNKNOWN | SATISFIED | UNSATISFIED)
  //
  int simplify (int rounds = 3);

  //------------------------------------------------------------------------
  // Force termination of 'solve' asynchronously.
  //
  //  require (SOLVING | READY)
  //  ensure (UNKNOWN)           // actually not immediately (synchronously)
  //
  void terminate ();

  //------------------------------------------------------------------------

  // We have the following common reference counting functions, which avoid
  // to restore clauses but require substantial user guidance.  This was the
  // only way to use inprocessing in incremental SAT solving in Lingeling
  // (and before in MiniSAT's 'freeze' / 'thaw') and which did not use
  // automatic clause restoring.  In general this is slower than
  // restoring clauses and should not be used.
  //
  // In essence the user freezes variables which potentially are still
  // needed in clauses added or assumptions used after the next 'solve'
  // call.  As in Lingeling you can freeze a variable multiple times, but
  // then have to melt it the same number of times again in order to enable
  // variable eliminating on it etc.  The arguments can be literals
  // (negative indices) but conceptually variables are frozen.
  //
  // In the old way of doing things without restore you should not use a
  // variable incrementally (in 'add' or 'assume'), which was used before
  // and potentially could have been eliminated in a previous 'solve' call.
  // This can lead to spurious satisfying assignment.  In order to check
  // this API contract one can use the 'checkfrozen' option.  This has the
  // drawback that restoring clauses implicitly would fail with a fatal
  // error message even if in principle the solver could just restore
  // clauses. Thus this option is disabled by default.
  //
  // See our SAT'19 paper [FazekasBiereScholl-SAT'19] for more details.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  bool frozen (int lit) const;
  void freeze (int lit);
  void melt (int lit);          // Also needs 'require (frozen (lit))'.

  //------------------------------------------------------------------------

  // Root level assigned variables can be queried with this function.
  // It returns '1' if the literal is implied by the formula, '-1' if its
  // negation is implied, or '0' if this is unclear at this point.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  int fixed (int lit) const;

  //------------------------------------------------------------------------
  // Force the default decision phase of a variable to a certain value.
  //
  void phase (int lit);
  void unphase (int lit);

  //------------------------------------------------------------------------

  // Enables clausal proof tracing in DRAT format and returns 'true' if
  // successfully opened for writing.  Writing proofs has to be enabled
  // before calling 'solve', 'add' and 'dimacs', that is in state
  // 'CONFIGURING'.  Otherwise only partial proofs would be written.
  //
  //   require (CONFIGURING)
  //   ensure (CONFIGURING)
  //
  bool trace_proof (FILE * file, const char * name); // Write DRAT proof.
  bool trace_proof (const char * path);              // Open & write proof.

  // Flush proof trace file.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  void flush_proof_trace ();

  // Close proof trace early.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  void close_proof_trace ();

  //------------------------------------------------------------------------

  static void usage (); // print usage information for long options

  static void configurations (); // print configuration usage options

  //   require (!DELETING)
  //   ensure (!DELETING)
  //
  void statistics ();   // print statistics
  void resources ();    // print resource usage (time and memory)

  //   require (VALID)
  //   ensure (VALID)
  //
  void options ();      // print current option and value list

  //------------------------------------------------------------------------
  // Traverse irredundant clauses or the extension stack in reverse order.
  //
  // The return value is false if traversal is aborted early due to one of
  // the visitor functions returning false.  See description of the
  // iterators below for more details on how to use these functions.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  bool traverse_clauses (ClauseIterator &) const;
  bool traverse_witnesses_backward (WitnessIterator &) const;
  bool traverse_witnesses_forward (WitnessIterator &) const;

  //------------------------------------------------------------------------
  // Files with explicit path argument support compressed input and output
  // if appropriate helper functions 'gzip' etc. are available.  They are
  // called through opening a pipe to an external command.
  //
  // If the 'strict' argument is zero then the number of variables and
  // clauses specified in the DIMACS headers are ignored, i.e., the header
  // 'p cnf 0 0' is always legal.  If the 'strict' argument is larger '1'
  // strict formatting of the header is required, i.e., single spaces
  // everywhere and no trailing white space.
  //
  // Returns zero if successful and otherwise an error message.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  const char * read_dimacs (FILE * file,
                            const char * name, int & vars, int strict = 1);

  const char * read_dimacs (const char * path, int & vars, int strict = 1);

  // The following routines work the same way but parse both DIMACS and
  // INCCNF files (with 'p inccnf' header and 'a <cube>' lines).  If the
  // parser finds and 'p inccnf' header or cubes then '*incremental' is set
  // to true and the cubes are stored in the given vector (each cube
  // terminated by a zero).

  const char * read_dimacs (FILE * file,
                            const char * name, int & vars, int strict,
                            bool & incremental, std::vector<int> & cubes);

  const char * read_dimacs (const char * path, int & vars, int strict,
                            bool & incremental, std::vector<int> & cubes);

  //------------------------------------------------------------------------
  // Write current irredundant clauses and all derived unit clauses
  // to a file in DIMACS format.  Clauses on the extension stack are
  // not included, nor any redundant clauses.
  //
  // The 'min_max_var' parameter gives a lower bound on the number '<vars>'
  // of variables used in the DIMACS 'p cnf <vars> ...' header.
  //
  // Returns zero if successful and otherwise an error message.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  const char * write_dimacs (const char * path, int min_max_var = 0);

  // The extension stack for reconstruction a solution can be written too.
  //
  const char * write_extension (const char * path);

  // Print build configuration to a file with prefix 'c '.  If the file
  // is '<stdout>' or '<stderr>' then terminal color codes might be used.
  //
  static void build (FILE * file, const char * prefix = "c ");

private:

  //==== start of state ====================================================

  State _state;            // API states as discussed above.

  /*----------------------------------------------------------------------*/

  // The 'Solver' class is a 'facade' object for 'External'.  It exposes the
  // public API of 'External' but hides everything else (except for the some
  // private functions).  It is supposed to make it easier to understand the
  // API and use the solver through the API.

  // This approach has the benefit of decoupling this header file from all
  // internal data structures, which is particularly useful if the rest of
  // the source is not available. For instance if only a CaDiCaL library is
  // installed in a system, then only this header file has to be installed
  // too, and still allows to compile and link against the library.

  /*----------------------------------------------------------------------*/

  // More precisely the CaDiCaL code is split into three layers:
  //
  //   Solver:       facade object providing the actual API of the solver
  //   External:     communication layer between 'Solver' and 'Internal'
  //   Internal:     the actual solver code
  //
  // The 'External' and 'Internal' layers are declared and implemented in
  // the corresponding '{external,internal}.{hpp,cpp}' files (as expected),
  // while the 'Solver' facade class is defined in 'cadical.hpp' (here) but
  // implemented in 'solver.cpp'.  The reason for this naming mismatch is,
  // that we want to use 'cadical.hpp' for the library header (this header
  // file) and call the binary of the stand alone SAT also 'cadical', which
  // is more naturally implemented in 'cadical.cpp'.
  //
  // Separating 'External' from 'Internal' also allows us to map external
  // literals to internal literals, which is useful with many fixed or
  // eliminated variables (during 'compact' the internal variable range is
  // reduced and external variables are remapped).  Such an approach is also
  // necessary, if we want to use extended resolution in the future (such as
  // bounded variable addition).
  //
  Internal * internal;     // Hidden internal solver.
  External * external;     // Hidden API to internal solver mapping.

#ifndef NTRACING
  // The API calls to the solver can be traced by setting the environment
  // variable 'CADICAL_API_TRACE' to point to the path of a file to which
  // API calls are written. The same format is used which 'mobical' can
  // read, execute and also shrink through delta debugging.
  //
  // The environment variable is read in the constructor and the trace is
  // opened for writing and then closed again in the destructor.
  //
  // Alternatively one case use 'trace_api_calls'.  Both
  //
  bool close_trace_api_file; // Close file if owned by solver it.
  FILE * trace_api_file;     // Also acts as flag that we are tracing.

  static bool tracing_api_through_environment;

  //===== end of state ====================================================

  void trace_api_call (const char *) const;
  void trace_api_call (const char *, int) const;
  void trace_api_call (const char *, const char *, int) const;
#endif

  void transition_to_unknown_state ();

  //------------------------------------------------------------------------
  // Used in the stand alone solver application 'App' and the model based
  // tester 'Mobical'.  So only these two classes need direct access to the
  // otherwise more application specific functions listed here together with
  // the internal DIMACS parser.

  friend class App;
  friend class Mobical;
  friend class Parser;

  // Read solution in competition format for debugging and testing.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  const char * read_solution (const char * path);

  // This gives warning messages for wrong 'printf' style format string usage.
  // Apparently (on 'gcc 9' at least) the first argument is 'this' here.
  // TODO: support for other compilers (beside 'gcc' and 'clang').
  //
# define CADICAL_ATTRIBUTE_FORMAT(FORMAT_POSITION,VARIADIC_ARGUMENT_POSITION) \
    __attribute__ ((format (printf, FORMAT_POSITION, VARIADIC_ARGUMENT_POSITION)))

  // Messages in a common style.
  //
  //   require (VALID | DELETING)
  //   ensure (VALID | DELETING)
  //
  void section (const char *);          // print section header
  void message (const char *, ...)      // ordinary message
                CADICAL_ATTRIBUTE_FORMAT (2, 3);

  void message ();                      // empty line - only prefix
  void error (const char *, ...)        // produce error message
              CADICAL_ATTRIBUTE_FORMAT (2, 3);

  // Explicit verbose level ('section' and 'message' use '0').
  //
  //   require (VALID | DELETING)
  //   ensure (VALID | DELETING)
  //
  void verbose (int level, const char *, ...)
                           CADICAL_ATTRIBUTE_FORMAT (3, 4);

  // Factoring out common code to both 'read_dimacs' functions above.
  //
  //   require (VALID)
  //   ensure (VALID)
  //
  const char * read_dimacs (File *, int &, int strict,
                            bool * incremental = 0,
                            std::vector<int> * = 0);

  // Factored out common code for 'solve', 'simplify' and 'lookahead'.
  //
  int call_external_solve_and_check_results (bool preprocess_only);

  //------------------------------------------------------------------------
  // Print DIMACS file to '<stdout>' for debugging and testing purposes,
  // including derived units and assumptions.  Since it will print in terms
  // of internal literals it is otherwise not really useful.  To write a
  // DIMACS formula in terms of external variables use 'write_dimacs'.
  //
  //   require (!INITIALIZING)
  //   ensure (!INITIALIZING)
  //
  void dump_cnf ();
  friend struct DumpCall; // Mobical calls 'dump_cnf' in 'DumpCall::execute'
};

/*========================================================================*/

// Connected terminators are checked for termination regularly.  If the
// 'terminate' function of the terminator returns true the solver is
// terminated synchronously as soon it calls this function.

class Terminator {
public:
  virtual ~Terminator () { }
  virtual bool terminate () = 0;
};

// Connected learners which can be used to export learned clauses.
// The 'learning' can check the size of the learn clause and only if it
// returns true then the individual literals of the learned clause are given
// to the learn through 'learn' one by one terminated by a zero literal.

class Learner {
public:
  virtual ~Learner () { }
  virtual bool learning (int size) = 0;
  virtual void learn (int lit) = 0;
};

class LearnSource {
public:
  virtual ~LearnSource () { }
  virtual bool hasNextClause () = 0;
  virtual const std::vector<int>& getNextClause () = 0;
};

class Rater {
public:
  virtual ~Rater () { }
  virtual bool rating () = 0;
  virtual void rate (const std::vector<Clause*>& clauses, const std::function<int(int)> externalize) = 0;
  virtual void clauseDeleted(Clause* clause) = 0;
};

/*------------------------------------------------------------------------*/

// Allows to traverse all remaining irredundant clauses.  Satisfied and
// eliminated clauses are not included, nor any derived units unless such
// a unit literal is frozen. Falsified literals are skipped.  If the solver
// is inconsistent only the empty clause is traversed.
//
// If 'clause' returns false traversal aborts early.

class ClauseIterator {
public:
  virtual ~ClauseIterator () { }
  virtual bool clause (const std::vector<int> &) = 0;
};

/*------------------------------------------------------------------------*/

// Allows to traverse all clauses on the extension stack together with their
// witness cubes.  If the solver is inconsistent, i.e., an empty clause is
// found and the formula is unsatisfiable, then nothing is traversed.
//
// The clauses traversed in 'traverse_clauses' together with the clauses on
// the extension stack are logically equivalent to the original clauses.
// See our SAT'19 paper for more details.
//
// The witness literals can be used to extend and fix an assignment on the
// remaining clauses to satisfy the clauses on the extension stack too.
//
// All derived units of non-frozen variables are included too.
//
// If 'witness' returns false traversal aborts early.

class WitnessIterator {
public:
  virtual ~WitnessIterator () { }
  virtual bool witness (const std::vector<int> & clause,
                        const std::vector<int> & witness) = 0;
};

/*------------------------------------------------------------------------*/

}

#endif
