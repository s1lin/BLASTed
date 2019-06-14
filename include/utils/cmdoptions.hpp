/** \file utils_cmdoptions.hpp
 * \brief Some helper functions for parsing options from PETSc's options database
 * \author Aditya Kashi
 * \date 2017-10
 */

#ifndef BLASTED_OPTIONPARSER_H
#define BLASTED_OPTIONPARSER_H

#include <stdexcept>
#include <vector>
#include <string>
#include <petscsys.h>

namespace blasted {

/// An exception to throw for errors from PETSc; takes a custom message
class Petsc_exception : public std::runtime_error
{
public:
	Petsc_exception(const std::string& msg);
	Petsc_exception(const char *const msg);
};

/// Exception thrown when a required input was not provided
class InputNotGivenError : public std::runtime_error
{
public:
	InputNotGivenError(const std::string& msg);
};

/// Throw an error from an error code related to petsc
/** \param ierr an expression which, if true, triggers the exception
 * \param str a short string message describing the error
 */
inline void petsc_throw(const int ierr, const std::string str) {
	if(ierr != 0) 
		throw Petsc_exception(str);
}

/// Checks whether a command line option has been passed irrespective of any argument values
bool parsePetscCmd_isDefined(const std::string optionname);

/// Extracts an integer corresponding to the argument from the default PETSc options database 
/** Throws an exception if the option was not set or if it could not be extracted.
 * \param optionname The name of the option to get the value of; needs to include the preceding '-'
 */
int parsePetscCmd_int(const std::string optionname);

/// Optionally extracts a real corresponding to the argument from the default PETSc options database 
/** Throws an exception if the function to read the option fails, but not if it succeeds and reports
 * that the option was not set.
 * \param optionname Name of the option to be extracted
 * \param defval The default value to be assigned in case the option was not passed
 */
PetscReal parseOptionalPetscCmd_real(const std::string optionname, const PetscReal defval);

/// Extracts a boolean corresponding to the argument from the default PETSc options database 
/** Throws an exception if the option was not set or if it could not be extracted.
 * \param optionname The name of the option to get the value of; needs to include the preceding '-'
 */
bool parsePetscCmd_bool(const std::string optionname);

/// Extracts a string corresponding to the argument from the default PETSc options database 
/** Throws a string exception if the option was not set or if it could not be extracted.
 * \param optionname The name of the option to get the value of; needs to include the preceding '-'
 * \param len The max number of characters expected in the string value
 */
std::string parsePetscCmd_string(const std::string optionname, const size_t len);

/// Extracts the arguments of an int array option from the default PETSc options database
/** \param maxlen Maximum number of entries expected in the array
 * \return The vector of array entries; its size is the number of elements read, no more
 */
std::vector<int> parsePetscCmd_intArray(const std::string optionname, const int maxlen);

/// Extracts the arguments of an int array option from the default PETSc options database
/** Does not throw if the requested option was not found; just returns an empty vector in that case. 
 * \param maxlen Maximum number of entries expected in the array
 */
std::vector<int> parseOptionalPetscCmd_intArray(const std::string optionname, const int maxlen);

}

#endif
