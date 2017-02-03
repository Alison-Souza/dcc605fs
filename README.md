# dcc605fs
Trabalho da disciplina Sistemas Operacionais sobre Sistemas de Arquivos.
Tarefas:

fs_format(fname, blocksize)
	verificar os erros EINVAL e ENOSPC
	calcular o numero de blocos usando o tamanho de fname.
	inicializar o FS com um diretório raiz vazio ("/").

fs_open(fname)
	abre o FS em fname e retorna o superbloco.
	verifica o erro EBADF (0xdcc605f5).
	não permitir que o FS seja aberto 2 vezes (EBUSY).

fs_close(sb)
	fecha o FS apontado por sb.
	libera a memória e recursos alocados à sb.
	retorna 0 em caso de sucesso.
	retorna negativo em caso de erro e verifica qual erro.
	verifica o erro EBADF (0xdcc605f5).

fs_get_block(sb)
	retorna um ponteiro (número) de um bloco livre em sb.
	retira o bloco retornado da lista de blocos livres.
	retorna 0 caso não existam mais blocos livres.
	retorna (uint64_t) -1 e atribui errno caso ocorra erro.

fs_put_block(sb, block)
	recoloca block na lista de blocos livres de sb.
	retorna 0 caso sucesso.
	retorna negativo caso erro e verifica errno.

fs_write_file(sb, fname, buf, cnt)
	escreve cnt bytes no FS de sb.
	escreve num arquivo chamado fname (se o arquivo existir, sobreescreve).
	fname deve conter um caminho absoluto.
	retorna 0 caso sucesso.
	retorna negativo caso erro, e verifica errno.

fs_read_file(sb, fname, buf, bufsz)
	lê os primeiros bufsz do arquivo fname e coloca no vetor apontado por buf.
	retorna a quantidade de bytes lidos em caso de sucesso.
	em caso de erro, atribui a errno o erro correspondente.

fs_unlink(sb, fname)
	remove o arquivo de nome fname do FS de sb, liberando os blocos associados.
	retorna 0 caso sucesso.
	retorna negativo caso erro e atualiza errno.

fs_mkdir(sb, dpath)
	cria um diretório no caminho dpath.
	dpath deve ser absoluto (começar com uma barra, não ter espaços).
	não precisa criar recursivamente, só cria uma pasta por vez.
	(para criar /x/y, se /x não exstir retorna ENOENT).
	retorna 0 caso sucesso.
	retorna negativo caso erro e atualiza errno.

fs_rmdir(sb, dname)
	remove o diretorio de nome dname.
	retorna 0 caso sucesso
	retorna negativo caso erro e atualiza errno (ENOTEMPTY se não estiver vazio)

fs_list_dir(sb, dname)
	retorna uma string com o nome de todos os elementos (arq e dir) em dname separados por espaços.
	os diretorios devem ter uma / ao final do nome.
	a ordem dos arquivos não importa.
	não foi dito se pode retornar erro (caso diretorio vazio, etc...)

References: 

open(), read(), write() and close()
ftp://gd.tuwien.ac.at/languages/c/programming-bbrown/c_075.htm

lseek()
http://pubs.opengroup.org/onlinepubs/000095399/functions/lseek.html

flock()
https://linux.die.net/man/2/flock
