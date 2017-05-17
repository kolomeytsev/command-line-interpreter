#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h> //open
#include <fcntl.h>

struct List {
	char* data;
	int special; //1 - special, 0 - not special
	struct List *next;
};

struct List_argv {
	char** elem_argv;
	struct List_argv *next2;
};

enum quot_flag {opened, closed};
enum flags_error {error_syntax, end_of_file, cont, fd_err};

char* need_more_space(char** p, int nArr)
{
	char* ptr;
	char* temp;
	int i;
	temp = *p;
	*p = malloc(10*nArr*sizeof(char));
	ptr = *p;
	for (i=0; i<(10*(nArr-1)); i++) {
		(*p)[i] = temp[i];
		ptr++;
	}
	free(temp);
	return ptr;
}

char* optimize_word(char* p, int nc)
{
	char* ptr;
	int i;
	ptr = malloc(nc*sizeof(char));
	for (i=0; i<nc; i++) //copy
		ptr[i] = p[i];
	free(p);
	return ptr;
}

struct List* add_new_elem(char* p, struct List* lst)
{
	struct List* lst_tmp, *tmp;
	tmp = lst;
	lst_tmp = malloc(sizeof(*lst));
	lst_tmp->data = p;
	lst_tmp->special = 0;
	lst_tmp->next = NULL;
	if (lst == NULL)
		lst = lst_tmp;
	else {
		while (tmp->next!=NULL)
			tmp = tmp->next;
		tmp->next = lst_tmp;
	}
	return lst;
}

struct List* add_special_sym(struct List* lst, int c)
{
	struct List* lst_tmp, *tmp;
	char* p;
	p = malloc(2*sizeof(char));
	p[0] = c;
	p[1] = '\0';
	tmp = lst;
	lst_tmp = malloc(sizeof(*lst));
	lst_tmp->data = p;
	lst_tmp->special = 1;
	lst_tmp->next = NULL;
	if (lst == NULL)
		lst = lst_tmp;
	else {
		while (tmp->next!=NULL)
			tmp = tmp->next;
		tmp->next = lst_tmp;
	}
	return lst;
}

struct List* add_special_sym2(struct List* lst)
{
	struct List *tmp;
	char* p;
	p = malloc(3*sizeof(char));
	p[0] = '>';
	p[1] = '>';
	p[2] = '\0';
	tmp = lst;
	while (tmp->next!=NULL)
		tmp = tmp->next;
	free(tmp->data);
	tmp->data = p;
	return lst;
}

int list_length(struct List* lst)
{
	int i = 0;
	while (lst!=NULL) {
		i++;
		lst = lst->next;
	}
	return i;
}

int find_amp(struct List* lst)
{
	if (lst!=NULL) {
		while (lst->next!=NULL)
			lst = lst->next;
		if ((lst->data[0]=='&')&&
			(lst->data[1]=='\0')&&
			(lst->special==1)
		)
			return 1;
	}
	return 0;
}

int find_conv(struct List* lst)
{
	if (lst!=NULL) {
		while (lst!=NULL) {
			if ((lst->data[0]=='|')&&
				(lst->data[1]=='\0')&&
				(lst->special==1)
			)
				return 1;
			lst = lst->next;
		}
	}
	return 0;
}

char** make_arr(struct List* lst, int size)
{
	int i = 0;
	char** argv;
	if (size!=0)
	argv = malloc((size+1) * sizeof(char*));
	while (lst!=NULL) {
		argv[i] = lst->data;
		i++;
		lst = lst->next;
		if (i==size)
			break;
	}
	argv[i] = NULL;
	return argv;
}		

void redirection(int *redirs)
{
	if (redirs[1]>-2) {
		dup2(redirs[1], 1);
		close(redirs[1]);
	}
	if (redirs[0]>-2) {
		dup2(redirs[0], 0);
		close(redirs[0]);
	}
}

void process(char** arr, int amp, int *redirs)
{
	int pid, p;
	pid = fork();
	if (pid==-1) {
		perror("fork");
		return;
	}
	if (pid==0) {
		redirection(redirs);
		execvp(arr[0], arr);
		perror(arr[0]);
		exit(1);
	}
	if (redirs[1]>-2)
		close(redirs[1]);
	if (redirs[0]>-2)
		close(redirs[0]);
	if (amp==0) { //not background
		p = wait(NULL);
		while (p>0) {
			if (p==pid)
				break;
			p = wait(NULL);
		}
	}
}

void kill_zombies()
{
	int p;
	p = wait4(-1, NULL, WNOHANG, NULL);
	while (p>0)
		p = wait4(-1, NULL, WNOHANG, NULL);
}

void cd_processing(char** argv, int sizeofarr)
{
	int a;
	if (sizeofarr==2) {
		a = chdir(argv[1]);
		if (a==-1)
			perror("cd");
	} else {
		printf("Error cd command\n");
	}
}

void killers(struct List* lst)
{
	if (lst!=NULL) {
		killers(lst->next);
		free(lst->data);
		free(lst);
	}
}

int read_word(char** p, int c, enum quot_flag* p_quot)
{
	int nc = 0, nArr = 1;
	char* ptr;
	ptr = *p = malloc(10*sizeof(char));
	while ((c!='\n')&&(c!=EOF)&&(c!=';')) {
		if ((c==' ')&&(*p_quot==closed))
			break;
		if (*p_quot==closed)
			if ((c=='>')||(c=='<')||(c=='|')||(c=='&'))
				break;
		if ((nc == (10*nArr - 1))&&(c!='\"')) {
			nArr++;
			*ptr = c;
			nc++;
			ptr = need_more_space(p, nArr);
			c = getchar();
		} else
		if ((nc < (10*nArr))&&(c!='\"')) {
			*ptr = c;
			nc++;
			ptr++;
			c = getchar();
		}
		while (c=='\"') {
			c = getchar();
			*p_quot = !(*p_quot);
		}
	}
	*ptr = '\0';
	*p = optimize_word(*p, (nc+1));
	return c;
}

struct List* read_special_sym(struct List* lst, int *c)
{
	int ch;
	lst = add_special_sym(lst, *c);
	ch = getchar();
	if ((ch=='>')&&(*c=='>')) {
		lst = add_special_sym2(lst);
		ch = getchar();
	}
	*c = ch;
	return lst;
}

struct List* read_str(enum flags_error* flag)
{
	int c;
	struct List* list_of_words = NULL;
	char** word;
	enum quot_flag quot = closed;
	c = getchar();
	while ((c!='\n')&&(c!=EOF)&&(c!=';')) {
		if (quot==closed) {
			if ((c=='&')||(c=='<')||(c=='>')||(c=='|'))
				list_of_words = read_special_sym(list_of_words, &c);
		}
		if (quot==closed)
			while (c==' ')		//skip space
				c = getchar();
		if (((c!='&')||(quot==opened))&&(c!='\n')) {
			word = malloc(sizeof(char*));
			c = read_word(word, c, &quot);
			if (c==EOF) {		//unexpected EOF
				*flag = end_of_file;
				printf("\nunexpected end of file\n");
				return NULL;
			}
			if (**word!='\0')
				list_of_words = add_new_elem(*word, list_of_words);
			else
				free(*word);
			free(word);
		}
	}
	if (c==EOF)
		*flag = end_of_file;
	if (quot==opened) {
		printf("unpaired quotes\n");
		killers(list_of_words);
		return NULL;
	}
	return list_of_words;
}

void kill_em_all(struct List_argv* lst)
{
	if (lst!=NULL) {
		kill_em_all(lst->next2);
		free(lst);
	}
}

void kill_conv_argv(struct List_argv* argvlst, int conv_num)
{
	int i;
	struct List_argv *argvlst2;
	argvlst2 = argvlst;
	for (i=0;i<conv_num;i++) {
		if ((argvlst2->elem_argv!=NULL)&&(*(argvlst2->elem_argv)!=NULL))
		{
			free(argvlst2->elem_argv);
			argvlst2->elem_argv = NULL;
		}
		argvlst2 = argvlst2->next2;
	}
	kill_em_all(argvlst);
}

int list_checking(struct List* lst) //2 special sym in row
{
	struct List* tmp;
	if (lst!=NULL) {
		if (lst->next!=NULL) {
			tmp = lst;
			lst = lst->next;
			while (lst!=NULL) {
				if ((tmp->special==lst->special)&&(lst->special==1))
					return 1;
				tmp = lst;
				lst = lst->next;
			}
		}
	}
	return 0;
}

int list_checking_2(struct List* lst) //special sym in the end only &
{
	if (lst!=NULL) {
		while (lst->next!=NULL)
			lst = lst->next;
		if (lst->special==1) {
			if (*(lst->data)!='&')
				return 1;
		}
	}
	return 0;
}

int list_checking_3(struct List* lst) //first sym is not | or &
{
	if (lst!=NULL)
		if (lst->special==1)
			if ((*(lst->data)=='|')||
				(*(lst->data)=='&')
			)
				return 1;
	return 0;
}

int list_checking_4(struct List* lst) //no sym after &
{
	while (lst!=NULL) {
		if ((lst->special==1)&&(*(lst->data)=='&'))
			if (lst->next!=NULL)
				return 1;
		lst = lst->next;
	}
	return 0;
}

int list_cult(struct List* lst, int *redirs)
{
	char *p;
	int eolist=0;
	redirs[0] = -2;
	redirs[1] = -2;
	while (lst!=NULL) {
		while (lst->special==0) {
			lst = lst->next;
			if (lst==NULL) {
				eolist = 1;
				break;
			}
		}
		if (eolist==0) {
			if (*(lst->data)=='>') {	//redir of write
				p = lst->next->data;
				if (lst->data[1]=='\0')
					redirs[1]=open(p,O_WRONLY|O_CREAT|O_TRUNC,0666);
				else
					redirs[1]=open(p,O_WRONLY|O_CREAT|O_APPEND,0666);
				if (redirs[1]==-1) {
					perror(p);
					return 1;
				}
			}
			if (*(lst->data)=='<') {	//redir of read
				p = lst->next->data;
				redirs[0]=open(p, O_RDONLY);
				if (redirs[0]==-1) {
					perror(p);
					return 1;
				}
			}
			if ((*(lst->data)=='&')||(*(lst->data)=='|'))
				lst = lst->next;
			else
				lst = lst->next->next;
		}
	}
	return 0;
}

void please_free_me(struct List *tmp)
{
	free(tmp->next->data);
	free(tmp->next);
	free(tmp->data);
	free(tmp);
}

struct List *make_new_list(struct List* lst)
{
	struct List *tmp, *first;
	tmp = lst;
	first = lst;
	if (lst!=NULL) {
		while ((lst->special==1)&&(lst->data[0]!='&')&&		//if ">..."
				(lst->data[0]!='|')
		) {	
			lst = lst->next->next;
			please_free_me(tmp);
			tmp = lst;
			if (lst==NULL)
				break;
		}
		first = lst;
	}
	if (lst!=NULL)
		while (lst->next!=NULL) {
			if ((lst->next->special==1)&&(lst->next->data[0]!='&')&&
				(lst->next->data[0]!='|')
			) {
				tmp = lst->next;
				lst->next = lst->next->next->next;
				please_free_me(tmp);
			} else
				lst = lst->next;
		}
	return first;
}

struct List* lst_without_amp(struct List* lst)
{
	struct List* tmp;
	tmp = lst;
	while (lst->next->next!=NULL)
		lst = lst->next;
	free(lst->next->data);
	free(lst->next);
	lst->next = NULL;
	return tmp;
}

int num_of_commands_in_conv(struct List* lst)
{
	int n=0;
	while (lst!=NULL) {
		if (lst->special==1)
			if (lst->data[0]=='|')
				n++;
		lst = lst->next;
	}
	n++;
	return n;
}

struct List_argv* argv_to_list(char** argv, struct List_argv* lst)
{
	struct List_argv* lst_tmp, *tmp;
	tmp = lst;
	lst_tmp = malloc(sizeof(*lst));
	lst_tmp->elem_argv = argv;
	lst_tmp->next2 = NULL;
	if (lst == NULL)
		lst = lst_tmp;
	else {
		while (tmp->next2!=NULL)
			tmp = tmp->next2;
		tmp->next2 = lst_tmp;
	}
	return lst;
}

char** make_argv_for_list(struct List* lst, int numofcom)
{
	int i=0, size=0, num=1;
	struct List* tmp;
	char** argv;
	while ((lst!=NULL)&&(num<numofcom)) { //skip
		while ((lst->data[0]!='|')&&(lst->special!=1)) {
			lst = lst->next;
			if (lst==NULL)
				break;
		}
		num++;
		lst = lst->next;
	}
	tmp = lst;
		while ((tmp->data[0]!='|')&&(tmp->special!=1)) {
			size++;
			tmp = tmp->next;
			if (tmp==NULL)
				break;
		}
	argv = malloc((size+1) * sizeof(char*));
	for (i=0;i<size;i++) {
		argv[i] = lst->data;
		lst = lst->next;
	}
	argv[i] = NULL;
	return argv;
}

struct List_argv *make_argv_list(struct List* lst, int conv_num)
{
	int i;
	struct List_argv* argv_lst = NULL;
	char** argv;
	for (i=1;i<=conv_num; i++) {
		argv = make_argv_for_list(lst, i);
		argv_lst = argv_to_list(argv, argv_lst);
	}
	return argv_lst;
}

void close_redirections(int *redirs)
{
	if (redirs[0]>-2)
		close(redirs[0]);
	if (redirs[1]>-2)
		close(redirs[1]);
}

void wait_all_pids(int conv_num, int *arr_pid)
{
	int p, s, i;
	p = wait(NULL);
	s = conv_num;
	while (s>0) {
		for (i=0; i<conv_num; i++)
			if (arr_pid[i]==p) {
				arr_pid[i] = 0;
				s--;
			}
		p = wait(NULL);
	}
	free(arr_pid);
}

void make_redirs(int i, int n, int *redirs)
{
	if ((redirs[0]>-2)&&(i==1)) {
		dup2(redirs[0], 0);
		close(redirs[0]);
	}
	if ((redirs[1]>-2)&&(i==n)) {
		dup2(redirs[1], 1);
		close(redirs[1]);
	}
}

void make_pipe_redirs(int *fd, int *fd_save, int i, int n)
{
	if (i!=n) { //redirection of stnd output
		close(fd[0]);
		dup2(fd[1], 1);
		close(fd[1]);
	}
	if (i!=1) { //redirection of stnd input
		close(fd_save[1]);
		dup2(fd_save[0], 0);
		close(fd_save[0]);
	}
}

void proc_conv(struct List_argv* argvlst, int num, int amp, int *redirs)
{
	int i, j=0, fd_save[2], err, pid, fd[2], *arr_pid;
	if (amp==0)
		arr_pid = malloc(num * sizeof(int));
	for (i=1;i<=num; i++) {
		if (i!=num) {
			err = pipe(fd);
			if (err==-1) {
				perror("pipe");
				exit(1);
			}
		}
		pid = fork();
		if (pid==-1) {
			perror("fork");
			exit(1);
		}
		if (pid > 0)
			if (amp==0) {
				arr_pid[j] = pid;
				j++;
			}
		if (pid==0) {
			make_redirs(i, num, redirs);
			make_pipe_redirs(fd, fd_save, i, num);
			execvp((argvlst->elem_argv)[0], argvlst->elem_argv);
			perror((argvlst->elem_argv)[0]);
			exit(1);
		}
		if (i!=1) {
			close(fd_save[0]);
			close(fd_save[1]);
		}
		argvlst = argvlst->next2;
		fd_save[0] = fd[0]; //save fd desctriptors
		fd_save[1] = fd[1];
	}
	close_redirections(redirs);
	if (amp==0)
		wait_all_pids(num, arr_pid);
}

int check_list(struct List* lst)
{
	int check;
	check = list_checking(lst); //2 special in row;0-OK, 1-ERROR
	if (check==0)
		check = list_checking_2(lst); //special sym in end is only &
	if (check==0)
		check = list_checking_3(lst); //first sym is not | or &
	if (check==0)
		check = list_checking_4(lst); //no sym after &
	return check;
}

void process_not_conveyor(struct List* lst, int amp, int *redirs)
{
	int sizeofarr;
	char** argv;
	if (lst!=NULL) {
		sizeofarr = list_length(lst);
		if (amp==1)
			sizeofarr--;
		argv = make_arr(lst, sizeofarr);
		if ((**argv=='c')&&(argv[0][1]=='d'))
			if (argv[0][2]=='\0')
				cd_processing(argv, sizeofarr);
			else
				process(argv, amp, redirs);
		else
			process(argv, amp, redirs);
	}
	if ((argv!=NULL)&&(*argv!=NULL))
		free(argv);
}

int main()
{
	enum flags_error flag = cont;
	struct List* lst;
	struct List_argv* argvlst;
	int amp, flag_conv, redirs[2], conv_num=0;
	printf(">");
	while (flag!=end_of_file) {
		lst = read_str(&flag);
		if (lst!=NULL) {
			if (check_list(lst)!=0) {
				flag = error_syntax;
				printf("syntax error\n");
			} else
				if (list_cult(lst, redirs))
					flag = fd_err;
			if (flag==cont) {
				if ((redirs[0]>-2)||(redirs[1]>-2))
					lst = make_new_list(lst);	// list without redir-s
				amp = find_amp(lst);
				flag_conv = find_conv(lst);
				if (flag_conv==1) {				// conveyor
					if (amp==1)
						lst = lst_without_amp(lst);
					conv_num = num_of_commands_in_conv(lst);
					argvlst = make_argv_list(lst, conv_num);
					proc_conv(argvlst, conv_num, amp, redirs);
					kill_conv_argv(argvlst, conv_num);
				} else 							//not conveyor
					process_not_conveyor(lst, amp, redirs);
				kill_zombies();
			}
		}
		killers(lst);
		printf(">");
		if (flag!=end_of_file)
			flag = cont;
	}
	return 0;
}
