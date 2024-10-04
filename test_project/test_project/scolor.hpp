#pragma once
/*
 * Color library for C++
 * Use like this;
 * std::cout << RED("This is in red!");
 * Some of the colors are just here for the sake of completeness (e.g. BBLACK)
 * In general, these have "normal", dark, and light versions.
 */

#include<string>

#define BLACK(X)	(std::string("\x1b[0;30m") + std::string(X) + std::string("\x1b[1;0m"))
#define RED(X)		(std::string("\x1b[0;31m") + std::string(X) + std::string("\x1b[1;0m"))
#define GREEN(X)	(std::string("\x1b[0;32m") + std::string(X) + std::string("\x1b[1;0m"))
#define YELLOW(X)	(std::string("\x1b[0;33m") + std::string(X) + std::string("\x1b[1;0m"))
#define BLUE(X)	(std::string("\x1b[0;34m") + std::string(X) + std::string("\x1b[1;0m"))
#define PURPLE(X)	(std::string("\x1b[0;35m") + std::string(X) + std::string("\x1b[1;0m"))  
#define CYAN(X)	(std::string("\x1b[0;36m") + std::string(X) + std::string("\x1b[1;0m"))
#define WHITE(X)	(std::string("\x1b[0;37m") + std::string(X) + std::string("\x1b[1;0m"))

#define BBLACK(X)		(std::string("\x1b[1;30m") + std::string(X) + std::string("\x1b[1;0m"))	
#define BRED(X)		(std::string("\x1b[1;31m") + std::string(X) + std::string("\x1b[1;0m"))
#define BGREEN(X)		(std::string("\x1b[1;32m") + std::string(X) + std::string("\x1b[1;0m"))	
#define BYELLOW(X)	(std::string("\x1b[1;33m") + std::string(X) + std::string("\x1b[1;0m"))	
#define BBLUE(X)		(std::string("\x1b[1;34m") + std::string(X) + std::string("\x1b[1;0m"))
#define BPURPLE(X)	(std::string("\x1b[1;35m") + std::string(X) + std::string("\x1b[1;0m"))	
#define BCYAN(X)		(std::string("\x1b[1;36m") + std::string(X) + std::string("\x1b[1;0m"))
#define BWHITE(X)		(std::string("\x1b[1;37m") + std::string(X) + std::string("\x1b[1;0m"))	
#define DBLACK(X)		(std::string("\x1b[2;30m") + std::string(X) + std::string("\x1b[1;0m"))	
#define DRED(X)		(std::string("\x1b[2;31m") + std::string(X) + std::string("\x1b[1;0m"))
#define DGREEN(X)		(std::string("\x1b[2;32m") + std::string(X) + std::string("\x1b[1;0m"))
#define DYELLOW(X)	(std::string("\x1b[2;33m") + std::string(X) + std::string("\x1b[1;0m"))
#define DBLUE(X)		(std::string("\x1b[2;34m") + std::string(X) + std::string("\x1b[1;0m"))
#define DPURPLE(X)	(std::string("\x1b[2;35m") + std::string(X) + std::string("\x1b[1;0m"))
#define DCYAN(X)		(std::string("\x1b[2;36m") + std::string(X) + std::string("\x1b[1;0m"))
#define DWHITE(X)		(std::string("\x1b[2;37m") + std::string(X) + std::string("\x1b[1;0m"))