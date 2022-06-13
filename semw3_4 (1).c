#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include<string.h>
#include <fcntl.h>

struct node {
	char *el;
	struct node *next;
};
typedef struct node Node;
typedef Node *list;

void printlist(list head) {
	if(head != NULL) {
		printf("%s\n", head -> el);
		printlist(head -> next);
	}
}

void pushback(list *headptr, char *word) {
	list p = malloc(sizeof(Node));
	p -> el = word;
	p -> next = NULL;
	list t = *headptr;
	if(*headptr == NULL) {
		*headptr = p;
		return;
	}
	while(t -> next != NULL) {
		t = t -> next;
	}
	t -> next = p;
}

int unique = 0,/// Как часто встретим амперсант?
conv = 0;///Создание контейнера (Считает количество | в буфере команд. Если будет больше 1 - ошибка)

char* retrieve(char *elem) {///Вводим команду и проверяем что с ней делать далее
////Если возвращаемое значение равно нулю - то это специальная команда &, | и т.д.
	char *word = NULL,/// возвращаемое слово
	ch;///специальная переменная для приема букв.
    int n = 0;///специальный счетчик чтобы формировать слово.
	
	ch = getchar();
	if (ch == EOF){///Конец файла?
		word = (char*)malloc(3);
		word[0] = 'E';
		word[1] = 'O';
		word[2] = 'F';
		return word;
	}
	if(ch == '|') {///Создать контейнер?
		conv++;
		*elem = ch;
		return NULL;
	}
	if(ch == '&'){///Создать фоновый режим?
		unique++;
		*elem = ch;
		return NULL;
	}
	if (ch == '\n' || ch == ' ') {///Закончили вводить слово?
		*elem = ch;
		return NULL;
	}
	if(ch != ' ') {//Мы еще вводим слово?
        	while(ch != '\n' && ch != '&' && ch != '|') {///Если это самое обычное слово без лишних знаков, то циклимся
			
				if(ch != '"'){///Если мы вводим единичное слово без обьединений с другими словами
			        word = realloc(word, (n + 1) * sizeof(char));
			        word[n] = ch;
			        n++;
			        ch = getchar();
					if (ch == ' ')///конец слова
						break;
				}
				else{///Если решили обьединить несколько слов в одно
					ch = getchar();
					while (ch != '"'){//Пока не закончим объединять
						if (ch == ' ')
							ch = getchar();
						else{
							word = realloc(word, (n + 1) * sizeof(char));
							word[n] = ch;
							n++;
							ch = getchar();
						}
					}
					ch = getchar();
				}
        	}
        	word = realloc(word, (n + 1) * sizeof(char));
		*elem = ch;
	}
	if(ch == '&')///ДАЛЕЕ МЫ СООБЩИМ ОБ ОШИБКЕ
		unique++;
	if(ch == '|') {///ДАЛЕЕ МЫ СООБЩИМ ОБ ОШИБКЕ
		conv++;
	}
	word[n] = '\0';
	return word;

}
int status;
void sigchld_handler(int signo) {
	waitpid(-1, &status, WNOHANG);
	if(WIFEXITED(status)){
		printf("I have done my work\n");
		printf("The process is exited with %u\n",WEXITSTATUS(status));
	}
}
void execution(list head) {
	int count = 0, ///Считаем количество комманд '|' в программе.
	bg, /// флаг наличия или отсутствия фонового режима
	x, ///Файловый дескриптор для выполнения фонового режима
	n = 0, 
	amp = 0, ///Количество амперсантов в списке комманд
	i = 0, ///специальный счетчик для конвейера buff
	k = 0, ///специальный счетчик для двух конвейеров buffa buffb.
	fconv = 0,///флаг наличия или остутсвия конвейера
	fd[2];

	if(pipe(fd) < 0){
		perror("ERROR PIPE");
		exit(1);
	}// pipe

	char **buff,///Обычный конвейер комманд
	**buffa,///Конвейер 1 
	**buffb;///Конвейер 2
	buffa = (char**)malloc(50);
	buffb = (char**)malloc(50);
	buff = NULL;
	list t = head;//Копия списка комманд
	
	while(t != NULL){
		if (t -> el[0] == '|'){
			fconv = 1;
			count++;
		}
		buff = realloc(buff, (i+1)*sizeof(char *));
		buff[i] = t -> el;
		i++;
		t = t -> next;
	}
	if (count > 1) {
		printf("Error: too many (|) symbols\n");
	}
	if(*buff[i-1] == '&') {
		bg = 1;
		buff[i-1] = '\0';
	}
	for (int m = 0; m < (i-1); m++) {
		if (*buff[m] == '&')
			amp++;
	}

	if((amp > 0) && (*buff[i-1] != '&'))
		printf("\nERROR: ENTER (&) SYMBOL AT THE END\n");
	
	buff = realloc(buff, (i + 1) * sizeof(char *));
	buff[i] = NULL;
	
	if (fconv == 1 && count == 1) {///Если есть конвейер! Разделим одну большой массив команд на две маленькие.
		while(*buff[k] != '|') {
			k++;
		}
		for(int m = 0; m < k; m++) {
			buffa[m] = buff[m];
		}
		buffa[k] = '\0';
		for(int m = k+1; m < i; m++) {
			buffb[n] = buff[m];
			n++;
		}
		buffb[n] = '\0';
	}
	pid_t pid,///Пид нашего процесса 
	wpid, ///Пид ожидания. Нужен для завершения родителя после завершения потомка
	gchpid;///Пид для конвейера
        pid = fork();
	if(pid != 0 && bg!= 1 && fconv == 1 && count == 1) {//Родитель. Отсутствие фонового режима и наличие конвейера
		while(!WIFEXITED(status) && !WIFSIGNALED(status))
			wpid = waitpid(pid, &status,WNOHANG);//Ждем окончания работы потомка
	}
	if(pid == 0 && bg != 1 && fconv == 1 && count == 1) {//Потомок. Отсутствие фонового режима и наличие конвейера. Выполнение конвейера.

		dup2(fd[1], 1);
		close(fd[0]);
		close(fd[1]);
		execvp(buffa[0], buffa);
		perror("execvp1");
		_exit(1);

	}
	if(fconv == 1){///Создание внутри потомка внука чтобы сын и внук выполняли конвейер
		if(gchpid = fork() < 0){
			perror("fork2");
			exit(1);
		}
	}
	if((!gchpid) && bg != 1 && fconv == 1 && count == 1) {///Внук. Отсутствие фонового режима и наличие конвейера. Выполнение конвейера.
		dup2(fd[0], 0);
		close(fd[0]);
		close(fd[1]);
		execvp(buffb[0], buffb);
		perror("execvp2");
		_exit(1);
	}

	if(pid != 0 && bg == 1 && fconv != 1){//Отец. Выполнение фонового режима без конвейера.
		signal(SIGCHLD, sigchld_handler);
    }

    if(pid == 0 && bg == 1 && fconv != 1){//Сын. Выполнение фонового режима без конвейера.
		signal(SIGINT,  SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTSTP, SIG_IGN);

                close(STDIN_FILENO);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);

        x = open("/dev/null", O_RDWR);   // Redirect input, output and stderr to /dev/null
        dup(x);
        dup(x);
        execvp(buff[0], buff);       // Execute background process - WILL GO INTO ZOMBIE/DEFUNCT STATE --
        _exit(1);
    }
    if (pid < 0) {
       perror("FORK ERROR");
       exit(1);
    }
        if(pid == 0 && bg != 1 && fconv != 1){///Сын. Обычное выполнение без фонового режима и без конвейера
                if(execvp(buff[0], buff) == -1) {
                       	perror("ERROR");
                       	exit(1);
                }
                execvp(buff[0], buff);
        }
        else if(pid != 0 && bg != 1 && fconv != 1){///Отец. Ждет выпонение сына.
                while(!WIFEXITED(status) && !WIFSIGNALED(status))
			wpid = waitpid(pid, &status,WNOHANG);
        }
}
void ERRORcd(list head) {
        if(head -> next == NULL)
                fprintf(stderr, "Expected  argument to \"cd\"\n");
        else {
                if(chdir(head -> next -> el) != 0)
                        perror("ERRORcd");
        }
        return;
}


int main() {
	while(1) {
		list head = NULL;///Список
		char *word = NULL;///Слово
		
		int a = 0;///Проверяем на ошибки.

		char elem = '\0';///Буква

		while(elem != '\n') {
			word = retrieve(&elem);
			if(word != NULL) {
				if(word[0] == 'E' && word[1] == 'O' && word[2] == 'F') {
					a = -1;
					break;
				}
				pushback(&head, word);
			}
			if(unique>0){
				char symbol[2] = {'&', '\0'};
				pushback(&head, symbol);
			}
			if(conv > 0) {
				char conv[2] = {'|', '\0'};
				pushback(&head, conv);
			}
			unique = 0;
			conv = 0;
		}
		if (a == -1)
			break;
		printlist(head);

		if(head -> el[0] == 'c' && head -> el[1] == 'd' && head -> el[2] == '\0') {
			ERRORcd(head);
			return 1;
		}
		execution(head);
	}
	return 0;
}
