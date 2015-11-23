int main(void){


#define PARSE(x) \
	if (strcmp(line_name, "global_"#x) == 0) { \
	gl->x = atoi(line_value); \
	continue; \
	}


	PARSE(zoom_absolute);

}

