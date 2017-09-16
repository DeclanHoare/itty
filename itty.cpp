// Itty - A BIT interpreter
// Copyright 2017 Declan Hoare
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <exception>
#include <algorithm>
#include <memory>
#include <variant>
#include <cctype>
#include <cstdlib>

class invalid_character_exception: public std::exception
{
	private:
		std::string msg;
	public:
		invalid_character_exception(char ch)
		{
			this->msg = "Encountered a non-uppercase-alphabetical, non-whitespace character: ";
			this->msg += ch;
			this->msg += " (" + std::to_string((int) ch) + ")";
		}
		const char* what() const throw()
		{
			return this->msg.c_str();
		}
};

class invalid_token_exception: public std::exception
{
	private:
		std::string msg;
	public:
		invalid_token_exception(std::string tok)
		{
			this->msg = "Encountered an invalid token: " + tok;
		}
		const char* what() const throw()
		{
			return this->msg.c_str();
		}
};

class unexpected_token_exception: public std::exception
{
	private:
		std::string msg;
	public:
		unexpected_token_exception(std::string tok, std::string area)
		{
			this->msg = "Encountered unexpected token " + tok + " while parsing " + area;
		}
		const char* what() const throw()
		{
			return this->msg.c_str();
		}
};

class unexpected_end_of_program_exception: public std::exception
{
	private:
		std::string msg;
	public:
		unexpected_end_of_program_exception(std::string area)
		{
			this->msg = "Unexpected end of program while parsing " + area;
		}
		const char* what() const throw()
		{
			return this->msg.c_str();
		}
};

class duplicate_goto_exception: public std::exception
{
	private:
		std::string msg;
	public:
		duplicate_goto_exception(size_t linenum)
		{
			this->msg = "Multiple gotos on line " + std::to_string(linenum) + " cover one condition";
		}
		const char* what() const throw()
		{
			return this->msg.c_str();
		}
};

class invalid_operation_exception: public std::exception
{
	private:
		std::string msg;
	public:
		invalid_operation_exception(std::string msg)
		{
			this->msg = "Invalid operation: " + msg;
		}
		const char* what() const throw()
		{
			return this->msg.c_str();
		}
};

const std::vector<std::string> token_strings = 
{
	"ZERO",
	"ONE",
	"LINENUMBER",
	"CODE",
	"GOTO",
	"IFTHEJUMPREGISTERIS",
	"THEJUMPREGISTER",
	"VARIABLE",
	"THEVALUEAT",
	"THEVALUEBEYOND",
	"THEADDRESSOF",
	"NAND",
	"EQUALS",
	"OPENPARENTHESIS",
	"CLOSEPARENTHESIS",
	"PRINT",
	"READ"
};

enum class token
{
	ZERO,
	ONE,
	LINENUMBER,
	CODE,
	GOTO,
	IFTHEJUMPREGISTERIS,
	THEJUMPREGISTER,
	VARIABLE,
	THEVALUEAT,
	THEVALUEBEYOND,
	THEADDRESSOF,
	NAND,
	EQUALS,
	OPENPARENTHESIS,
	CLOSEPARENTHESIS,
	PRINT,
	READ
};

enum class expression_type
{
	THE_JUMP_REGISTER,
	VARIABLE,
	THE_VALUE_AT,
	THE_VALUE_BEYOND,
	THE_ADDRESS_OF,
	NAND
};

struct expression
{
	expression_type type;
	std::variant<std::monostate, size_t, std::shared_ptr<expression>> lvalue, rvalue;
};

struct equals_command
{
	std::shared_ptr<expression> var;
	std::variant<std::monostate, size_t, std::shared_ptr<expression>> val;
};

struct goto_def
{
	bool enabled = false;
	size_t destination;
};

struct line
{
	bool enabled = false;
	size_t line_number; // For error reporting
	
	// This variant contains an std::monostate for read commands, a bool
	// for print commands, or an equals_command for equals commands.
	std::variant<std::monostate, bool, equals_command> command;
	
	goto_def goto_unconditional, goto_true, goto_false;
};

std::vector<bool> memory;

enum class variable_type
{
	EMPTY,
	BIT,
	ADDRESS_OF_A_BIT
};

struct variable
{
	variable_type type = variable_type::EMPTY;
	size_t index; // value for addresses
};

std::vector<variable> variables;

bool jump_register = false;
std::vector<line> program;

bool strict_mode = false;

std::vector<token> lex(std::istream& fobj)
{
	std::vector<token> ret;
	std::vector<std::string>::const_iterator it;
	std::string tok = "";
	int ch;
	while ((ch = fobj.get()) != -1)
	{
		if (std::isspace(ch))
			continue;
		if (!std::isupper(ch))
			throw invalid_character_exception(ch);
		tok += (char) ch;
		it = std::find(token_strings.begin(), token_strings.end(), tok);
		if (it != token_strings.end())
		{
			ret.push_back((token) std::distance(token_strings.begin(), it));
			tok = "";
		}
	}
	if (tok == "")
		return ret;
	it = std::find(token_strings.begin(), token_strings.end(), tok);
	if (it == token_strings.end())
		throw invalid_token_exception(tok);
	ret.push_back((token) std::distance(token_strings.begin(), it));
	return ret;
}

size_t read_literal(std::vector<token>& code)
{
	size_t ret = 0;
	token tok;
	bool read_digit = false, read_token = false;
	while (!code.empty())
	{
		bool digit = false;
		tok = code.back();
		read_token = true;
		if (tok == token::ZERO || tok == token::ONE)
		{
			ret <<= 1;
			if (tok == token::ONE)
				ret ^= 1;
			digit = read_digit = true;
			code.pop_back();
		}
		if (!digit)
			break;
	}
	if (!read_token)
		throw unexpected_end_of_program_exception("literal number");
	if (!read_digit)
		throw unexpected_token_exception(token_strings[(size_t) tok], "literal number");
	return ret;
}

std::variant<std::monostate, size_t, std::shared_ptr<expression>> read_expression(std::vector<token>& code, bool in_parentheses = false)
{
	token tok;
	if (code.empty())
		throw unexpected_end_of_program_exception("expression");
	tok = code.back();
	std::variant<std::monostate, size_t, std::shared_ptr<expression>> expr;
	switch (tok)
	{
		case token::ZERO:
		case token::ONE:
		{
			expr = read_literal(code);
			break;
		}
		case token::THEJUMPREGISTER:
		{
			code.pop_back();
			expr = std::make_shared<expression>();
			std::get<std::shared_ptr<expression>>(expr)->type = expression_type::THE_JUMP_REGISTER;
			break;
		}
		case token::VARIABLE:
		{
			code.pop_back();
			expr = std::make_shared<expression>();
			std::get<std::shared_ptr<expression>>(expr)->type = expression_type::VARIABLE;
			std::get<std::shared_ptr<expression>>(expr)->rvalue = read_literal(code);
			break;
		}
		case token::THEVALUEAT:
		{
			code.pop_back();
			expr = std::make_shared<expression>();
			std::get<std::shared_ptr<expression>>(expr)->type = expression_type::THE_VALUE_AT;
			std::get<std::shared_ptr<expression>>(expr)->rvalue = read_expression(code);
			break;
		}
		case token::THEVALUEBEYOND:
		{
			code.pop_back();
			expr = std::make_shared<expression>();
			std::get<std::shared_ptr<expression>>(expr)->type = expression_type::THE_VALUE_BEYOND;
			std::get<std::shared_ptr<expression>>(expr)->rvalue = read_expression(code);
			break;
		}
		case token::THEADDRESSOF:
		{
			code.pop_back();
			expr = std::make_shared<expression>();
			std::get<std::shared_ptr<expression>>(expr)->type = expression_type::THE_ADDRESS_OF;
			std::get<std::shared_ptr<expression>>(expr)->rvalue = read_expression(code);
			break;
		}
		case token::OPENPARENTHESIS:
		{
			code.pop_back();
			expr = read_expression(code, true);
			break;
		}
		default:
			throw unexpected_token_exception(token_strings[(size_t) tok], "expression");
	}
	if (!code.empty())
	{
		tok = code.back();
		if (tok == token::NAND)
		{
			code.pop_back();
			std::shared_ptr<expression> nand = std::make_shared<expression>();
			nand->type = expression_type::NAND;
			nand->lvalue = expr;
			nand->rvalue = read_expression(code);
			expr = nand;
		}
	}
	if (in_parentheses && !code.empty())
	{
		tok = code.back();
		code.pop_back();
		if (tok != token::CLOSEPARENTHESIS)
			throw unexpected_token_exception(token_strings[(size_t) tok], "parentheses");
	}
	return expr;
}

void read_gotos(std::vector<token>& code, line& lin)
{
	token tok;
	if (code.empty())
		return;
	tok = code.back();
	if (tok == token::LINENUMBER)
		return;
	if (lin.goto_unconditional.enabled)
		throw duplicate_goto_exception(lin.line_number);
	if (tok != token::GOTO)
		throw unexpected_token_exception(token_strings[(size_t) tok], "goto");
	code.pop_back();
	if (code.empty())
		throw unexpected_end_of_program_exception("goto");
	size_t linenum = read_literal(code);
	goto_def* definition = &lin.goto_unconditional;
	if (!code.empty() && (code.back() == token::IFTHEJUMPREGISTERIS))
	{
		code.pop_back();
		if (code.empty())
			throw unexpected_end_of_program_exception("goto");
		tok = code.back();
		switch (tok)
		{
			case token::ZERO:
				code.pop_back();
				definition = &lin.goto_false;
				break;
			case token::ONE:
				code.pop_back();
				definition = &lin.goto_true;
				break;
		}
	}
	else
		if (lin.goto_true.enabled || lin.goto_false.enabled)
			throw duplicate_goto_exception(lin.line_number);
	if (definition->enabled)
		throw duplicate_goto_exception(lin.line_number);
	definition->enabled = true;
	definition->destination = linenum;
	if (!code.empty() && code.back() == token::GOTO)
		read_gotos(code, lin);
}

std::vector<line> parse(std::vector<token>& code)
{
	std::vector<line> ret;
	std::reverse(code.begin(), code.end());
	token tok;
	while (!code.empty())
	{
		tok = code.back();
		code.pop_back();
		if (tok != token::LINENUMBER)
			throw unexpected_token_exception(token_strings[(size_t) tok], "line number");
		size_t line_number = read_literal(code);
		if (code.empty())
			throw unexpected_end_of_program_exception("line number");
		tok = code.back();
		code.pop_back();
		if (tok != token::CODE)
			throw unexpected_token_exception(token_strings[(size_t) tok], "line number");
		if (code.empty())
			throw unexpected_end_of_program_exception("line");
		line lin;
		lin.enabled = true;
		lin.line_number = line_number;
		tok = code.back();
		switch (tok)
		{
			case token::PRINT:
			{
				code.pop_back();
				if (code.empty())
					throw unexpected_end_of_program_exception("line");
				tok = code.back();
				code.pop_back();
				switch (tok)
				{
					case token::ZERO:
						lin.command = false;
						break;
					case token::ONE:
						lin.command = true;
						break;
					default:
						throw unexpected_token_exception(token_strings[(size_t) tok], "print command");
				}
				break;
			}
			case token::READ: // no parameters
			{
				code.pop_back();
				if (code.empty())
					throw unexpected_end_of_program_exception("line");
				break;
			}
			default: // equals (has two operands) or error
			{
				equals_command eq;
				eq.var = std::get<std::shared_ptr<expression>>(read_expression(code));
				if (code.empty())
					throw unexpected_end_of_program_exception("line");
				tok = code.back();
				code.pop_back();
				if (tok != token::EQUALS)
					throw unexpected_token_exception(token_strings[(size_t) tok], "line");
				if (code.empty())
					throw unexpected_end_of_program_exception("line");
				eq.val = read_expression(code);
				if (std::holds_alternative<size_t>(eq.val) && std::get<size_t>(eq.val) > 1)
					throw invalid_operation_exception("used multi-bit literal in EQUALS command");
				if (std::holds_alternative<std::shared_ptr<expression>>(eq.val) && std::get<std::shared_ptr<expression>>(eq.val)->type == expression_type::THE_JUMP_REGISTER)
				{
					if (strict_mode)
						throw invalid_operation_exception("used THE JUMP REGISTER on right-hand side of EQUALS command on line " + std::to_string(line_number));
					else
						std::cerr << "warning: THE JUMP REGISTER on right-hand side of EQUALS command on line " << line_number << std::endl;
				}
				lin.command = eq;
				break;
			}
		}
		read_gotos(code, lin);
		if (line_number >= ret.size())
			ret.resize(line_number + 1);
		ret[line_number] = lin;
	}
	return ret;
}

enum class partial_bit
{
	EMPTY,
	Z,
	ZE,
	ZER,
	O,
	ON
};

bool read_bit()
{
	char in;
	partial_bit tok = partial_bit::EMPTY;
	while (true)
	{
		std::cin >> in;
		if (std::isspace(in))
			continue;
		switch (in)
		{
			case 'O':
			{
				if (tok == partial_bit::ZER)
					return false;
				else
					tok = partial_bit::O;
				break;
			}
			case 'N':
			{
				if (tok == partial_bit::O)
					tok = partial_bit::ON;
				else
					tok = partial_bit::EMPTY;
				break;
			}
			case 'E':
			{
				if (tok == partial_bit::ON)
					return true;
				else if (tok == partial_bit::Z)
					tok = partial_bit::ZE;
				else
					tok = partial_bit::EMPTY;
				break;
			}
			case 'Z':
			{
				tok = partial_bit::Z;
				break;
			}
			case 'R':
			{
				if (tok == partial_bit::ZE)
					tok = partial_bit::ZER;
				else
					tok = partial_bit::EMPTY;
				break;
			}
		}
	}
}

size_t get_address(std::shared_ptr<expression>& expr);

std::variant<std::monostate, bool, size_t> get_val(std::shared_ptr<expression>& expr)
{
	switch (expr->type)
	{
		case expression_type::VARIABLE:
		{
			size_t id = std::get<size_t>(expr->rvalue);
			variable var = variables[id];
			switch (var.type)
			{
				case variable_type::EMPTY:
					throw invalid_operation_exception("accessed uninitialised memory");
				case variable_type::BIT:
					return memory[var.index];
				case variable_type::ADDRESS_OF_A_BIT:
					return var.index;
				default:
					throw invalid_operation_exception("get_val(): this is bad; id = " + std::to_string(id) + ", variables.size() = " + std::to_string(variables.size()) + ", var.type = " + std::to_string((size_t) var.type));
			}
		}
		case expression_type::THE_ADDRESS_OF:
			return get_address(std::get<std::shared_ptr<expression>>(expr->rvalue));
		case expression_type::THE_VALUE_AT:
		case expression_type::THE_VALUE_BEYOND:
			return memory[get_address(expr)];
		case expression_type::THE_JUMP_REGISTER:
			return jump_register;
		case expression_type::NAND:
			return !((std::holds_alternative<std::shared_ptr<expression>>(expr->lvalue) ? std::get<bool>(get_val(std::get<std::shared_ptr<expression>>(expr->lvalue))) : (std::get<size_t>(expr->lvalue) != 0)) && (std::holds_alternative<std::shared_ptr<expression>>(expr->rvalue) ? std::get<bool>(get_val(std::get<std::shared_ptr<expression>>(expr->rvalue))) : (std::get<size_t>(expr->rvalue) != 0)));
		default:
			throw invalid_operation_exception("get_val(): this is bad. expr->type = " + std::to_string((size_t) expr->type));
	}
}

size_t get_address(std::shared_ptr<expression>& expr)
{
	switch (expr->type)
	{
		case expression_type::VARIABLE:
		{
			size_t index = std::get<size_t>(expr->rvalue);
			if (index >= variables.size())
			{
				variables.resize(index + 1);
				variables[index].type = variable_type::BIT;
				variables[index].index = memory.size();
			}
			if (variables[index].type == variable_type::ADDRESS_OF_A_BIT)
				throw invalid_operation_exception("tried to take the address of an address variable");
			return variables[index].index;
		}
		case expression_type::THE_VALUE_AT:
			return std::get<size_t>(get_val(std::get<std::shared_ptr<expression>>(expr->rvalue)));
		case expression_type::THE_VALUE_BEYOND:
			return std::get<size_t>(get_val(std::get<std::shared_ptr<expression>>(expr->rvalue))) + 1;
		default:
			throw invalid_operation_exception("get_address(): this is bad. expr->type = " + std::to_string((size_t) expr->type));
	}
}

void execute(std::vector<line>& program)
{
	size_t i = 0;
	line lin;
	do
		lin = program[i++];
	while (!lin.enabled);
	while (true)
	{
		if (std::holds_alternative<std::monostate>(lin.command)) // READ
			jump_register = read_bit();
		else if (std::holds_alternative<bool>(lin.command)) // PRINT
			std::cout << (std::get<bool>(lin.command) ? "ONE" : "ZERO") << std::endl;
		else // EQUALS
		{
			equals_command eq = std::get<equals_command>(lin.command);
			std::variant<std::monostate, bool, size_t> val;
			if (std::holds_alternative<size_t>(eq.val))
				val = std::get<size_t>(eq.val) != 0; // literal in equals must be bool but is stored as size_t
			else
				val = get_val(std::get<std::shared_ptr<expression>>(eq.val));
			if (eq.var->type == expression_type::THE_JUMP_REGISTER)
				jump_register = std::get<bool>(val);
			else
			{
				if (std::holds_alternative<size_t>(val))
				{
					size_t index = std::get<size_t>(eq.var->rvalue);
					if (index >= variables.size())
						variables.resize(index + 1);
					if (variables[index].type == variable_type::BIT)
						throw invalid_operation_exception("tried to place an address in a bit variable");
					variables[index].type = variable_type::ADDRESS_OF_A_BIT;
					variables[index].index = std::get<size_t>(val);
				}
				else
				{
					size_t index = get_address(eq.var);
					if (index >= memory.size())
						memory.resize(index + 1);
					memory[index] = std::get<bool>(val);
				}
			}
		}
		
		if (lin.goto_unconditional.enabled)
			lin = program[lin.goto_unconditional.destination];
		else if (lin.goto_false.enabled && !jump_register)
			lin = program[lin.goto_false.destination];
		else if (lin.goto_true.enabled && jump_register)
			lin = program[lin.goto_true.destination];
		else
			break;
	}
}

int main(int argc, char** argv)
{
	std::vector<std::string> args(argv, argv + argc);
	std::vector<std::string>::iterator it = std::find(args.begin(), args.end(), "--strict");
	if (it != args.end())
	{
		args.erase(it);
		strict_mode = true;
	}
	if (args.size() != 2)
	{
		std::cerr << "usage: " << args[0] << " [--strict] PROGRAM" << std::endl;
		return EXIT_FAILURE;
	}
	std::ifstream fobj(args[1]);
	if (!fobj.is_open())
	{
		std::cerr << "failed to open " << args[1] << std::endl;
		return EXIT_FAILURE;
	}
	std::vector<token> tokens = lex(fobj);
	fobj.close();
	std::vector<line> program = parse(tokens);
	execute(program);
	return EXIT_SUCCESS;
}

