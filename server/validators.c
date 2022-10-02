#include <stdbool.h>

typedef enum EmailSection {LOCAL, DOMAIN, TLD} EmailSection;

const static bool LOCAL_ALLOWED_CHARS[255] = {
  [33]          = true, // !
  [35  ... 39]  = true, // # $ % & '
  [42  ... 43]  = true, // * +
  [45  ... 47]  = true, // - . /
  [48  ... 57]  = true, // 0-9
  [61]          = true, // =
  [63  ... 64]  = true, // ? @
  [65  ... 90]  = true, // A-Z
  [94  ... 96]  = true, // ^ _ `
  [97  ... 122] = true, // a-z
  [123 ... 126] = true  // { | } ~
};

const static bool DOMAIN_ALLOWED_CHARS[255] = {
  [45  ... 46]  = true, // - .
  [48  ... 57]  = true, // 0-9
  [65  ... 90]  = true, // A-Z
  [97  ... 122] = true  // a-z
};

bool is_email_valid(char email[EMAIL_SIZE]){
  EmailSection section = LOCAL;
  int section_length = 0;
  char last = 0;
  char current = 0;
  for(int i=0; i < strlen(email); i++){
    current = email[i];
    if(section == LOCAL && !LOCAL_ALLOWED_CHARS[current]){
      return false; // current character is not on the allowed characters map
    }

    if(current == '.' && last == 0){
      return false; // . is not allowed at the start of a section
    }

    if(current == '.' && last == '.'){
      return false; // two . in sequence are not allowed
    }

    if(section == DOMAIN || section == TLD) {
      if(!DOMAIN_ALLOWED_CHARS[current]){
        return false; // current character is not on the allowed characters map
      }

      if(current == '-' && last == '-'){
        return false; // Two dashes cannot be used sequencially in the domain
      }

      if(current == '-' && last == 0){
        return false; // - is not allowed at the start of the domain
      }
    }

    if(current == '.' && section == DOMAIN){
      if(last == '-'){
        return false; // - is not valid before at the end of the domain
      }
      section = TLD;
    }

    if(current == '@'){
      if(section == LOCAL){
        if(last == '.'){
          return false; // . is not allowed at the end local section
        }
        if(section_length == 0){
          return false; // sections need to have characters
        }
        section = DOMAIN;
        last = 0;
        section_length = 0;
        continue;
      }
      else{
        return false; // @ is not allowed in the domain section
      }
    }

    last = current;
    section_length++;
  }

  if(section != TLD){
    return false;
  }

  if(section_length == 0){
    return false; // sections need to have characters
  }

  if(current == '.' || current == '-'){
    return false; // . and - are not allowed at the end of the domain section
  }

  return true;
}
