/*-------------------------------------------------------------------------
    This source file is a part of Clover
    For the latest info, see https://github.com/cmarrin/Clover
    Copyright (c) 2021-2022, Chris Marrin
    All rights reserved.
    Use of this source code is governed by the MIT license that can be
    found in the LICENSE file.
-------------------------------------------------------------------------*/

#include "Scanner.h"

#include <map>

using namespace clvr;

Token Scanner::scanString(char terminal)
{    
	uint8_t c;
	_tokenString.clear();
	
	while ((c = get()) != C_EOF) {
		if (c == terminal) {
			break;
		}
        while (c == '\\') {
            switch((c = get())) {
                case 'a': c = 0x07; break;
                case 'b': c = 0x08; break;
                case 'f': c = 0x0c; break;
                case 'n': c = 0x0a; break;
                case 'r': c = 0x0d; break;
                case 't': c = 0x09; break;
                case 'v': c = 0x0b; break;
                case '\\': c = 0x5c; break;
                case '\'': c = 0x27; break;
                case '"': c = 0x22; break;
                case '?': c = 0x3f; break;
                case 'u':
                case 'x': {
                    if ((c = get()) == C_EOF) {
                        return Token::String;
                    }
                    
                    if (!isHex(c) && !isdigit(c)) {
                        c = '?';
                        break;
                    }
                    
                    uint32_t num = 0;
                    putback(c);
                    while ((c = get()) != C_EOF) {
                        if (!isHex(c) && !isdigit(c)) {
                            break;
                        }
                        if (isdigit(c)) {
                            num = (num << 4) | (c - '0');
                        } else if (isUpper(c)) {
                            num = (num << 4) | ((c - 'A') + 0x0a);
                        } else {
                            num = (num << 4) | ((c - 'a') + 0x0a);
                        }
                    }
                    if (num > 0xffffff) {
                        _tokenString += static_cast<uint8_t>(num >> 24);
                        _tokenString += static_cast<uint8_t>(num >> 16);
                        _tokenString += static_cast<uint8_t>(num >> 8);
                        _tokenString += static_cast<uint8_t>(num);
                    } else if (num > 0xffff) {
                        _tokenString += static_cast<uint8_t>(num >> 16);
                        _tokenString += static_cast<uint8_t>(num >> 8);
                        _tokenString += static_cast<uint8_t>(num);
                    } else if (num > 0xff) {
                        _tokenString += static_cast<uint8_t>(num >> 8);
                        _tokenString += static_cast<uint8_t>(num);
                    } else {
                        _tokenString += static_cast<uint8_t>(num);
                    }
                    break;
                }
                default:
                    c = '?';
                    break;
            }
        }
		_tokenString += c;
	}
	return Token::String;
}

Token Scanner::scanIdentifier()
{
	uint8_t c;
	_tokenString.clear();

	bool first = true;
	while ((c = get()) != C_EOF) {
		if (!((first && isIdFirst(c)) || (!first && isIdOther(c)))) {
			putback(c);
			break;
		}
		_tokenString += c;
		first = false;
	}
    size_t len = _tokenString.size();
    return len ? Token::Identifier : Token::EndOfFile;
}

// Return the number of digits scanned
int32_t Scanner::scanDigits(int32_t& number, bool hex)
{
	uint8_t c;
    int32_t radix = hex ? 16 : 10;
    int32_t numDigits = 0;
    
	while ((c = get()) != C_EOF) {
		if (isdigit(c)) {
            number = number * radix;
            number += static_cast<int32_t>(c - '0');
        } else if (hex && isLCHex(c)) {
            number = number * radix;
            number += static_cast<int32_t>(c - 'a' + 10);
        } else if (hex && isUCHex(c)) {
            number = number * radix;
            number += static_cast<int32_t>(c - 'A' + 10);
        } else {
			putback(c);
			break;
        }
        ++numDigits;
	}
    return numDigits;
}

static float pow10(int32_t e)
{
    bool neg = false;
    if (e < 0) {
        neg = true;
        e = -e;
    }

    float r = 1;
    float p = 10;
    
    while (true) {
        if (e % 2 == 1) {
            r *= p;
            e--;
        }
        if (e == 0) {
            break;
        }
        p *= p;
        e /= 2;
    }
    return neg ? (1 / r) : r;
}
Token Scanner::scanNumber(TokenType& tokenValue)
{
	uint8_t c = get();
    if (c == C_EOF) {
        return Token::EndOfFile;
    }

	if (!isdigit(c)) {
		putback(c);
		return Token::EndOfFile;
	}
	
    bool hex = false;
    int32_t number = static_cast<int32_t>(c - '0');
    int32_t exp = 0;

    if (c == '0') {
        if ((c = get()) == C_EOF) {
            tokenValue.integer = static_cast<uint32_t>(number);
            return Token::Integer;
        }
        if (c == 'x' || c == 'X') {
            if ((c = get()) == C_EOF) {
                return Token::EndOfFile;
            }
            if (!isxdigit(c)) {
                putback(c);
                return Token::Unknown;
            }
            hex = true;
        }
        putback(c);
	}
    
    scanDigits(number, hex);
    if (scanFloat(number, exp)) {
        tokenValue.number = float(number) * pow10(exp);
        return Token::Float;
    }
    assert(exp == 0);
    tokenValue.integer = static_cast<uint32_t>(number);
    return Token::Integer;
}

bool Scanner::scanFloat(int32_t& mantissa, int32_t& exp)
{
    bool haveFloat = false;
	uint8_t c;
    if ((c = get()) == C_EOF) {
        return false;
    }
    if (c == '.') {
        haveFloat = true;
        exp = -scanDigits(mantissa, false);
        if ((c = get()) == C_EOF) {
            return true;
        }
    }
    if (c == 'e' || c == 'E') {
        haveFloat = true;
        if ((c = get()) == C_EOF) {
            return false;
        }
        int32_t neg = 1;
        if (c == '+' || c == '-') {
            if (c == '-') {
                neg = -1;
            }
        } else {
            putback(c);
        }
        int32_t realExp = 0;
        scanDigits(realExp, false);
        exp += neg * realExp;
    } else {
        putback(c);
    }
    return haveFloat;
}

Token Scanner::scanComment()
{
	uint8_t c;

	if ((c = get()) == '*') {
		for ( ; ; ) {
			c = get();
			if (c == C_EOF) {
				return Token::EndOfFile;
			}
			if (c == '*') {
				if ((c = get()) == '/') {
					break;
				}
				putback(c);
			}
		}
		return Token::Comment;
	}
	if (c == '/') {
		// Single line comment
		for ( ; ; ) {
			c = get();
			if (c == C_EOF) {
				return Token::EndOfFile;
			}
			if (c == '\n') {
                // Don't eat the newline for single line comments. We need to return it later
                putback(c);
				break;
			}
		}
		return Token::Comment;
	}

    _tokenString.clear();
    _tokenString += c;
	return Token('/');
}

Token Scanner::scanSpecial()
{
	uint8_t c = get();
    if (!isSpecial(c)) {
        putback(c);
        return Token::EndOfFile;
    }
    
    // See if it's a 2 char sequence
    static std::map<std::string, Token> specialSequence = {
        { "++",         Token::Inc },
        { "--",         Token::Dec },
        { "+=",         Token::AddSto },
        { "-=",         Token::SubSto },
        { "*=",         Token::MulSto },
        { "/=",         Token::DivSto },
        { "&=",         Token::AndSto },
        { "|=",         Token::OrSto },
        { "^=",         Token::XorSto },
        { "||",         Token::LOr },
        { "&&",         Token::LAnd },
        { "==",         Token::EQ },
        { "!=",         Token::NE },
        { "<=",         Token::LE },
        { ">=",         Token::GE },
    };

    char buf[3];
    buf[0] = c;
    buf[1] = get();
    buf[2] = '\0';
    auto it = specialSequence.find(buf);
    if (it == specialSequence.end()) {
        putback(buf[1]);
        return static_cast<Token>(c);
    }

    return it->second;
}

uint8_t Scanner::get() const
{
    if (_lastChar != C_EOF) {
        uint8_t c = _lastChar;
        _lastChar = C_EOF;
        return c;
    }
    int result = _stream->get();
    _charno++;
    
    if (_stream->eof() || result < 0 || result > 0xff || result == EOF) {
        return C_EOF;
    }
    uint8_t c = static_cast<uint8_t>(result);
    if (_annotations) {
        _annotations->back().second.push_back(char(c));
    }
    
    if (c == '\n') {
        ++_lineno;
        _charno = 1;
        if (_annotations) {
            _annotations->emplace_back(-1, "");
        }
    }
    return c;
}

Token Scanner::getToken(TokenType& tokenValue)
{
	uint8_t c;
	Token token = Token::EndOfFile;	
	while (token == Token::EndOfFile && (c = get()) != C_EOF) {
        if (isspace(c)) {
            continue;
        }
        
		switch(c) {
            case '\n':
                if (_ignoreNewlines) {
                    token = Token::EndOfFile;
					break;
                }
                token = Token::NewLine;
				break;
			case '/':
				token = scanComment();
				if (token == Token::Comment) {
					// For now we ignore comments
                    token = Token::EndOfFile;
					break;
				}
				break;
				
			case '\"':
			case '\'':
				token = scanString(c);
                tokenValue.str = _tokenString.c_str();
				break;

			default:
				putback(c);
				if ((token = scanSpecial()) != Token::EndOfFile) {                    
					break;
				}
				if ((token = scanNumber(tokenValue)) != Token::EndOfFile) {
					break;
				}
				if ((token = scanIdentifier()) != Token::EndOfFile) {
                    if (token == Token::Identifier) {
                        tokenValue.str = _tokenString.c_str();
                    }
					break;
				}
				token = Token::Unknown;
                break;
		}
	}
    
	return token;
}
