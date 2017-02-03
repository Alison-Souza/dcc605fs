#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <unistd.h>

#include "fs.h"

int NLINKS;
//======================== FUNÇÕES AUXILIARES ========================//
/*
Operations Modes
0: Procura e retorna pelo arq/dir no caminho fname.
   ex: /path/name , retorna o inode de file
1: Procura e retorna o diretorio pai do caminho.
   ex: /path/file , retorna o inode de path

*/
// Retorna o índice do bloco do arquivo que tenha o nome fname.
uint64_t find_block(struct superblock *sb, const char *fname, int opmode)
{

	if(opmode == 1)
	{
		char * lastbar = strrchr(fname,'\\');
		*lastbar = '\0';
	}

	// Fila dos blocos a serem percorridos.
	uint64_t* fila =  (uint64_t*) malloc (sb->blks * sizeof(uint64_t));
	// Fila que marca se um bloco foi visitado ou não.
	int* visitado = (int*) malloc (sb->blks * sizeof(int));
	int inicio = 0, fim = 0, i, aux;

	// Zerando as duas filas.
	for(i = 0; i < sb->blks; i++)
	{
		fila[i] = 0;
		visitado[i] = 0;
	}
	// Colocando o inode pasta raiz no início da fila, marcando ela
	// como visitada e incrementando o tamanho da fila.
	fila[inicio] = sb->root;
	visitado[sb->root] = 1;
	fim++;

	struct inode in;
	struct nodeinfo ni;
	while(inicio < fim)
	{
		// Colocando o ponteiro na posição indicada pelo início da fila.
		lseek(sb->fd, (fila[inicio] * sb->blksz), SEEK_SET);
		// Lendo os dados do início da fila.
		aux = read(sb->fd, &in, sb->blksz);

		// Se o inode in for de um arquivo regular (não é filho).
		if(in.mode == IMREG)
		{
			// Posicionando o ponteiro na posição do nodeinfo.
			lseek(sb->fd, ((in.meta)*sb->blksz), SEEK_SET);
			// Lendo o nodeinfo.
			aux = read(sb->fd, &ni, sb->blksz);
			// Se o nome do arquivo é igual ao parâmetro proocurado.
			if(strcmp(ni.name, fname) == 0)
			{
				// Libera os recursos e retorna o índice no FS.
				aux = fila[inicio];
				free(fila);
				free(visitado);
				return aux;
			}
		}
		// Se o inode in for de uma pasta.
		if(in.mode == IMDIR)
		{
			// Posicionando o ponteiro na posição do nodeinfo.
			lseek(sb->fd, ((in.meta)*sb->blksz), SEEK_SET);
			// Lendo o nodeinfo.
			aux = read(sb->fd, &ni, sb->blksz);

			// Se o nome do arquivo é igual ao parâmetro proocurado.
			if(strcmp(ni.name, fname) == 0)
			{
				// Libera os recursos e retorna o índice no FS.
				aux = fila[inicio];
				free(fila);
				free(visitado);
				return aux;
			}

			// Para cada elemento da pasta.
			for(i = 0; i < ni.size; i++)
			{
				// Se esse elemento não foi visitado.
				if(visitado[in.links[i]] == 0)
				{
					// Marca ele como visitado.
					visitado[in.links[i]] = 1;
					// Insere ele no final da fila.
					fila[fim] = in.links[i];
					// Incrementa o final da fila.
					fim++;
				}
			}
		}
		inicio++;
	}

	free(fila);
	free(visitado);
	// Caso erro, retorna -1.
	return (uint64_t) -1;
}

/*
*
*
*/
int link_block(struct superblock *sb, struct inode *in, uint64_t in_n, uint64_t block)
{
    int ii;
    uint64_t aux,iaux_n,n;
    struct inode *iaux = (struct inode*) malloc (sb->blksz);

    if(in->next == 0)
    {
        //Percorre para axar um local vazio
        for(ii=0;ii<NLINKS;ii++)
        {
            if(in->links[ii] == 0)
            {
                in->links[ii] = block;
                return 0;
            }
        }

        //Cria um novo inode
        n = fs_get_block(sb); //precisa checar o retorno?
        in->next = n;
        iaux->mode = IMCHILD;
        iaux->parent = in_n;
        iaux->next = 0;
        iaux->meta = in_n;
        iaux->links[0] = block;

        //escreve o novo inode
        lseek(sb->fd, n*sb->blksz, SEEK_SET);
        aux = write(sb->fd, iaux, sb->blksz);
        if(aux == -1) return -1;
        return 0;
    }

    while(in->next != 0)
    {
        iaux_n = in->next;
        lseek(sb->fd, iaux_n*sb->blksz, SEEK_SET);
        aux = read(sb->fd, &iaux, sb->blksz);
        in = iaux;
    }

    //Percorre para axar um local vazio
    for(ii=0;ii<NLINKS;ii++)
    {
        if(in->links[ii] == 0)
        {
            in->links[ii] = block;
            //escreve o inode de volta
            lseek(sb->fd, iaux_n*sb->blksz, SEEK_SET);
            aux = write(sb->fd, iaux, sb->blksz);
            return 0;
        }
    }

    //Cria um novo inode
    n = fs_get_block(sb); //precisa checar o retorno?
    in->next = n;
    iaux->mode = IMCHILD;
    iaux->parent = in_n;
    iaux->next = 0;
    iaux->meta = iaux_n;
    iaux->links[0] = block;

    //escreve o novo inode
    lseek(sb->fd, n*sb->blksz, SEEK_SET);
    aux = write(sb->fd, iaux, sb->blksz);
    if(aux == -1) return -1;
    return 0;
    }
//====================================================================//

/* Build a new filesystem image in =fname (the file =fname should be
 * present in the OS's filesystem).  The new filesystem should use
 * =blocksize as its block size; the number of blocks in the filesystem
 * will be automatically computed from the file size.  The filesystem
 * will be initialized with an empty root directory.  This function
 * returns NULL on error and sets errno to the appropriate error code.
 * If the block size is smaller than MIN_BLOCK_SIZE bytes, then the
 * format fails and the function sets errno to EINVAL.  If there is
 * insufficient space to store MIN_BLOCK_COUNT blocks in =fname, then
 * the function fails and sets errno to ENOSPC. */
struct superblock * fs_format(const char *fname, uint64_t blocksize)
{
	NLINKS = (blocksize - (4 * sizeof(uint64_t)))/sizeof(uint64_t);
	// Verifica se o tamanho do bloco é menor que o mínimo.
	if(blocksize < MIN_BLOCK_SIZE)
	{
		errno = EINVAL;
		return NULL;
	}

	/*
	//===========================================================//
	// É NECESSÁRIO O FLOCK AQUI ??????????????????????????????  //
	//===========================================================//

	// Pega o descritor de arquivo do FS.
	int fd = open(fname, O_RDWR);

	// Aplica uma trava exclusiva no arquivo (LOCK_EX = exclusive lock).
	// Apenas um processo poderá usar esse arquivo de cada vez.
	// flock retorna 0 se sucesso, -1 se erro (errno já é atribuido ??).
	if((flock(fd, LOCK_EX | LOCK_NB)) == -1)
	{
		errno = EBUSY;
		return NULL;
	}

	// BLABLABLA

	// LOCK_UN: remove a trava do arquivo.
	if(flock(fd, LOCK_UN | LOCK_NB) == -1)
	{
		errno = EBUSY;
		return NULL;
	}
	close(fd);
	*/

	// Calcula o tamanho do arquivo fname.
	FILE* fp = fopen(fname, "r");
	long fsize;

	if(fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		fsize = ftell(fp);
	}
	fclose(fp);

	// Verifica se o número de blocos é menor que o mínimo.
	int blocknum = fsize / blocksize;
	if(blocknum < MIN_BLOCK_COUNT)
	{
		errno = ENOSPC;
		return NULL;
	}

	// Declara o superbloco.
	struct superblock* sb = (struct superblock*) malloc (blocksize);
	sb->magic = 0xdcc605f5; // Definição.
	sb->blks = blocknum; // Número de blocos do sistema de arquivos.
	sb->blksz = blocksize; // Tamanho dos blocos (bytes).

	// Em um sistema de arquivos vazio existe o superbloco, o nodeinfo
	// da pasta raiz e o inode da pasta raiz, isso ocupa 3 blocos.
	// Número de blocos livres no sistema de arquivos.
	sb->freeblks = blocknum-3;
	// Apontador para o primeiro bloco livre.
	sb->freelist = 3;
	// Apontador para o inode da pasta raiz.
	sb->root = 2;
	// Descritor de arquivos do FS.
	sb->fd = open(fname, O_RDWR, S_IWRITE | S_IREAD);
	if(sb->fd == -1)
	{
		errno = EBADF;
		return NULL;
	}

	// Inicializando o superbloco.
	int aux = write(sb->fd, sb, sb->blksz);
	if(aux == -1) return NULL;

	// Inicializando a pasta raiz.
	struct nodeinfo* root_info = (struct nodeinfo*) malloc (sb->blksz);
	root_info->size = 0;
	strcpy(root_info->name, "/\0");
	aux = write(sb->fd, root_info, sb->blksz);
	free(root_info);

	struct inode* root_inode = (struct inode*) malloc (sb->blksz);
	root_inode->mode = IMDIR;
	root_inode->parent = 0;
	root_inode->meta = 1;
	root_inode->next = 0;
	aux = write(sb->fd, root_inode, sb->blksz);
	free(root_inode);

	// Inicializando a lista de blocos vazios.
	struct freepage* root_fp = (struct freepage*)
									malloc (sizeof(struct freepage*));
	for(int i = sb->freelist; i < sb->blks; i++)
	{
		if(i == (sb->blks - 1))
		{
			root_fp->next = 0; // Última pagina livre.
		}
		else
		{
			root_fp->next = i+1;
		}
		aux = write(sb->fd, root_fp, sb->blksz);
	}
	free(root_fp);

	return sb;
}

/* Open the filesystem in =fname and return its superblock. Returns NULL
 * on error, and sets errno accordingly.  If =fname does not contain a
 * 0xdcc605f5, then errno is set to EBADF. */
struct superblock * fs_open(const char *fname)
{
	// Pega o descritor de arquivo do FS.
	int fd = open(fname, O_RDWR);

	// Aplica uma trava exclusiva no arquivo (LOCK_EX = exclusive lock).
	// Apenas um processo poderá usar esse arquivo de cada vez.
	// flock retorna 0 se sucesso, -1 se erro (errno já é atribuido ??).
	if((flock(fd, LOCK_EX | LOCK_NB)) == -1)
	{
		errno = EBUSY;
		return NULL;
	}

	// Movo o leitor para a posição inicial do arquivo.
	lseek(fd, 0, SEEK_SET);

	// Carrego o superbloco do FS.
	struct superblock* block = (struct superblock*)
									malloc(sizeof(struct superblock));
	if(read(fd, block, sizeof(struct superblock)) == -1 )
		return NULL;

	// Verifico o erro EBADF.
	if(block->magic != 0xdcc605f5)
	{
		// LOCK_UN: remove a trava do arquivo.
		flock(fd, LOCK_UN | LOCK_NB);
		close(fd);
		errno = EBADF;
		return NULL;
	}

	return block;
}

/* Close the filesystem pointed to by =sb.  Returns zero on success and
 * a negative number on error. If there is an error, all resources are
 * freed and errno is set appropriately. */
int fs_close(struct superblock *sb)
{
	// Verifica o descritor do sistema de arquivos.
	if(sb->magic != 0xdcc605f5)
	{
		errno = EBADF;
		return -1;
	}

	// LOCK_UN: remove a trava do arquivo.
	if(flock(sb->fd, LOCK_UN | LOCK_NB) == -1)
	{
		errno = EBUSY;
		return -1;
	}

	// É necessário essa verificação???
	int aux = close(sb->fd);
	if(aux == -1) return -1;
	free(sb);

	return 0;
}

/* Get a free block in the filesystem.  This block shall be removed from
 * the list of free blocks in the filesystem.  If there are no free
 * blocks, zero is returned.  If an error occurs, (uint64_t)-1 is
 * returned and errno is set appropriately. */
uint64_t fs_get_block(struct superblock *sb)
{
	// Verifica o descritor do sistema de arquivos.
	if(sb->magic != 0xdcc605f5)
	{
		errno = EBADF;
		return (uint64_t) -1;
	}

	// Verificando se há blocos livres.
	if(sb->freeblks == 0)
	{
		errno = ENOSPC;
		return 0;
	}

	struct freepage *page = (struct freepage*) malloc (sb->blksz);
	// Localizando posição do primeiro bloco livre.
	lseek(sb->fd, (sb->freelist * sb->blksz), SEEK_SET);
	// Verificando se há algum erro na leitura
	int aux = read(sb->fd, page, sb->blksz);
	if(aux == -1) return (uint64_t) -1;
	// Pegando o "ponteiro" do bloco a ser retornado.
	uint64_t block = sb->freelist;
	// Mudando o ponteiro de lista vazia para o próximo bloco livre.
	sb->freelist = page->next;
	// Decrementando a quantidade de blocos livres.
	sb->freeblks--;

	// Escrevendo os novos dados do super bloco (freelist e freeblks).
	lseek(sb->fd, 0, SEEK_SET);
	aux = write(sb->fd, sb, sb->blksz);
	if(aux == -1) return (uint64_t) -1;

	free(page);
	return block;
}

/* Put =block back into the filesystem as a free block.  Returns zero on
 * success or a negative value on error.  If there is an error, errno is
 * set accordingly. */
int fs_put_block(struct superblock *sb, uint64_t block)
{
	// Verifica o descritor do sistema de arquivos.
	if(sb->magic != 0xdcc605f5)
	{
		errno = EBADF;
		return -1;
	}

	// Novo bloco a ser inserido na lista de blocos livres.
	struct freepage *page = (struct freepage*) malloc (sb->blksz);

	// Setando os ponteiros do bloco, a nova posição da tabela de blocos
	// livres e incrementando a quantidade de blocos livres.
	page->next = sb->freelist;
	sb->freelist = block;
	sb->freeblks++;

	// Escrevendo os novos dados do super bloco (freelist e freeblks).
	lseek(sb->fd, 0, SEEK_SET);
	int aux = write(sb->fd, sb, sb->blksz);
	if(aux == -1) return -1;

	// Escrevendo o bloco (page) no arquivo.
	lseek(sb->fd, block * sb->blksz, SEEK_SET);
	aux = write(sb->fd, page, sb->blksz);
	if(aux == -1) return -1;

	free(page);
	return 0;
}

int fs_write_file(struct superblock *sb, const char *fname, char *buf,
				  size_t cnt)
{
	// Verifica o descritor do sistema de arquivos.
	if(sb->magic != 0xdcc605f5)
	{
		errno = EBADF;
		return -1;
	}

	// Verifica se o nome do arquivo (caminho) é maior que o permitido.
	if(strlen(fname) > ((sb->blksz) - (8*sizeof(uint64_t))))
	{
		errno = ENAMETOOLONG;
		return -1;
	}

/*	// Verifica se o arquivo existe no FS.
	uint64_t block = find_block(sb, fname, 0);
	if(block > 0)
	{
		// Chama a função unlink.
	}
*/
	// FALTA MUITA COISA AQUI. MAS FAÇA FS_UNLINK PRIMEIRO.
	//verificar se existe algum arquivo com o nome fname no FS.
	//caso exista, esse arquivo deve ser apagado (fs_unlink).
	//navegue pelo caminho fname procurando onde inserir o arquivo.
	//crie o arquivo e escreva cnt bytes do buf.
	//retorna negativo caso erro, e verifica errno.
	//retorna 0 caso sucesso.

	return -1;
}

ssize_t fs_read_file(struct superblock *sb, const char *fname,
					 char *buf, size_t bufsz)
{
	// Verifica o descritor do sistema de arquivos.
	if(sb->magic != 0xdcc605f5)
	{
		errno = EBADF;
		return -1;
	}

	// Verifica se o nome do arquivo (caminho) é maior que o permitido.
	if(strlen(fname) > ((sb->blksz) - (8*sizeof(uint64_t))))
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	// Verifica se o arquivo existe no FS e salvo o "endereço" dele.
	uint64_t block = find_block(sb, fname, 0);
	if(block < 0)
	{
		errno = ENOENT;
		return -1;
	}

	struct inode in;
	struct nodeinfo ni;
	int aux, bufaux, nlinks, i, mod;
	char* reader = (char*) malloc (sb->blksz);
	// Posiciono o ponteiro na posição do inode do arquivo que vou ler.
	lseek(sb->fd, block * sb->blksz, SEEK_SET);
	// Carrego o inode.
	aux = read(sb->fd, &in, sb->blksz);

	// Verifica se o arquivo não é uma pasta/diretório.
	if(in.mode == IMDIR)
	{
		errno = EISDIR;
		free (reader);
		return -1;
	}

	// Posiciono o ponteiro na posição do nodeinfo desse arquivo.
	lseek(sb->fd, in.meta * sb->blksz, SEEK_SET);
	// Carrego o nodeinfo.
	aux = read(sb->fd, &ni, sb->blksz);

	// Quantos links tem 1 inode cheio.
	nlinks = (sb->blksz - 4 * sizeof(uint64_t)) / sizeof(uint64_t);
	// bufaux salva quantos bytes foream lidos.
	bufaux = 0;

	// Enquanto houver mais inodes e não passou do bufsz.
	while(in.next > 0 && bufaux < bufsz)
	{
		// Para todos os links do inode, se não passou de bufsz.
		for(i = 0; i < nlinks && bufaux < bufsz; i++)
		{
			// Posiciono no link[i]
			lseek(sb->fd, in.links[i] * sb->blksz, SEEK_SET);
			// Leio o link[i] numa variavel auxiliar reader.
			aux = read(sb->fd, reader, sb->blksz);
			// Concateno a reader com o buf.
			strcat(buf, reader);
			// Atualizo a quantidade de bytes lidos.
			bufaux += sizeof(sb->blksz);
		}
		// Posiciono e leio o próximo inode.
		lseek(sb->fd, in.next * sb->blksz, SEEK_SET);
		aux = read(sb->fd, &in, sb->blksz);
	}
	// No último inode.
	int nlinks_uin = ((ni.size % sb->blksz) / sizeof(uint64_t)) - 4;
	for(i = 0; i < nlinks_uin && bufaux < bufsz; i++)
	{
		// Se o bufsz for múltiplo de sb->blksz.
		if(((uint64_t) bufsz) % (sb->blksz) == 0)
		{
			// Posiciono no link[i]
			lseek(sb->fd, in.links[i] * sb->blksz, SEEK_SET);
			// Leio o link[i] numa variavel auxiliar reader.
			aux = read(sb->fd, reader, sb->blksz);
			// Concateno a reader com o buf.
			strcat(buf, reader);
			// Atualizo a quantidade de bytes lidos.
			bufaux += sb->blksz;
		}
		else // Se o bufsz não for múltiplo de sb->blksz.
		{
			// Calculo o mod em bufsz e sb->blksz.
			mod = ((uint64_t) bufsz) % (sb->blksz);
			// Posiciono no link[i]
			lseek(sb->fd, in.links[i] * sb->blksz, SEEK_SET);
			// Leio o link[i] numa variavel auxiliar reader.
			aux = read(sb->fd, reader, mod);
			// Concateno a reader com o buf.
			strcat(buf, reader);
			// Atualizo a quantidade de bytes lidos.
			bufaux += mod;
		}
	}
	free(reader);
	return bufaux;
}

int fs_unlink(struct superblock *sb, const char *fname)
{
	// Verifica o descritor do sistema de arquivos.
	if(sb->magic != 0xdcc605f5)
	{
		errno = EBADF;
		return -1;
	}

	// Verifica se o nome do arquivo (caminho) é maior que o permitido.
	if(strlen(fname) > ((sb->blksz) - (8*sizeof(uint64_t))))
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	// Verifica se o arquivo existe no FS.
	uint64_t block = find_block(sb, fname, 0);
	if(block < 0)
	{
		errno = ENOENT;
		return -1;
	}

	struct inode curr_in;
	struct inode next_in;
	struct nodeinfo ni;
	int aux;
	int i, index;

	// Posicionando o ponteiro na posição do primeiro inode para ler.
	lseek(sb->fd, block*sb->blksz, SEEK_SET);
	aux = read(sb->fd, &curr_in, sb->blksz);

	// Verifica se é uma pasta.
	if(curr_in.mode == IMDIR)
	{
		errno = EISDIR;
		return -1;
	}

	// Lendo o nodeinfo para pegar o tamanho do arquivo.
	lseek(sb->fd, curr_in.meta * sb->blksz, SEEK_SET);
	aux = read(sb->fd, &ni, sb->blksz);
	// Pegando o número de bytes do arquivo.
	int fsize = ni.size;

	// Liberando o nodeinfo desse arquivo.
	fs_put_block(sb, curr_in.meta);

	// Se houver mais de um inode.
	while(curr_in.next > 0)
	{
		// Libero todos os links desse inode
		// (todos estão em uso, já que tem mais inode).
		for(i = 0; i < NLINKS; i++)
		{
			fs_put_block(sb, curr_in.links[i]);
		}

		// Leio o próximo inode.
		lseek(sb->fd, curr_in.next*sb->blksz, SEEK_SET);
		aux = read(sb->fd, &next_in, sb->blksz);
		// Salvo o indice do próximo inode.
		index = curr_in.next;

		// Libero o inode corrente.
		fs_put_block(sb, block);

		//Atualizo o indice do bloco corrente.
		block = index;
		curr_in = next_in;
	}

	//TRATAR AQUI O CASO DE ÚLTIMO INODE
	// Calculo quantos links tem o último inode.
	int nlinks_uin = (fsize % sb->blksz) / sizeof(uint64_t);
	// Removendo o mode, parent, meta e next do inode
	// deixando apenas os links.
	nlinks_uin = nlinks_uin - 4;
	for(i = 0; i < nlinks_uin; i++)
	{
		// Libero os links.
		fs_put_block(sb, curr_in.links[i]);
	}

	// Libero o último inode.
	fs_put_block(sb, block);

	// Caso sucesso.
	return 0;
}

int fs_mkdir(struct superblock *sb, const char *dname)
{
	int aux;
	// Verifica o descritor do sistema de arquivos.
	if(sb->magic != 0xdcc605f5)
	{
		errno = EBADF;
		return -1;
	}

	// Verifica se o nome do arquivo (caminho) é maior que o permitido.
	if(strlen(dname) > ((sb->blksz) - (8*sizeof(uint64_t))))
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	//Erro se o caminho dname nao comeca com \ e se tem espaco
	if((*dname != '\\') || (strchr(dname,' ') != NULL) )
	{
		errno = ENOENT;
		return -1;
	}

	//Erro se o diretorio ja existe
	uint64_t block = find_block(sb, dname, 0);
	if(block > 0)
	{
		errno = EEXIST;
		return -1;
	}

    uint64_t parent_n = find_block(sb,dname, 1);
    //Erro, dir pai n existe
    if(parent_n < 0)
    {
        errno = ENOENT;
        return -1;
    }

    //Cria estrutura inode e nodeinfo do novo diretorio
    uint64_t dir_n,dirni_n;
    dir_n = fs_get_block(sb);
    dirni_n = fs_get_block(sb);
    if(dirni_n == -1 || dir_n == -1) return -1;
    struct inode parentdir;
    struct inode dir;
    struct nodeinfo dirni, parent_ni;
    dir.mode = IMDIR;
    dir.next = 0;
    dir.parent = parent_n;
    dir.meta = dirni_n;
    char *auxc = strrchr(dname,'\\');
    strcpy(dirni.name,auxc);
    dirni.size = 0;

     //Le o inode diretorio pai
     lseek(sb->fd, parent_n*sb->blksz, SEEK_SET);
     aux = read(sb->fd, &parentdir, sb->blksz);

     //linka o inode do novo dir ao dir pai
     link_block(sb,&parentdir,parent_n,dir_n);

      //Le o nodeinfo diretorio pai para alterar o numero de arquivos e escreve de volta
     lseek(sb->fd, parentdir.meta*sb->blksz, SEEK_SET);
     aux = read(sb->fd, &parent_ni, sb->blksz);
     parent_ni.size++;
     lseek(sb->fd, parentdir.meta*sb->blksz, SEEK_SET);
	 aux = write(sb->fd,&parent_ni,sb->blksz);

	 //escreve de volta o inode do pai
	 lseek(sb->fd, parent_n*sb->blksz, SEEK_SET);
	 aux = write(sb->fd,&parentdir,sb->blksz);

	 //Escreve o inode e nodeinfo do novo diretorio
	 lseek(sb->fd, dir_n*sb->blksz, SEEK_SET);
	 aux = write(sb->fd,&dir,sb->blksz);
	 lseek(sb->fd, dirni_n*sb->blksz, SEEK_SET);
	 aux = write(sb->fd,&dirni,sb->blksz);

	return 0;
}

int fs_rmdir(struct superblock *sb, const char *dname)
{
	return -1;
}

char * fs_list_dir(struct superblock *sb, const char *dname)
{
		// Verifica o descritor do sistema de arquivos.
	if(sb->magic != 0xdcc605f5)
	{
		errno = EBADF;
		return NULL;
	}

	// Verifica se o nome do arquivo (caminho) é maior que o permitido.
	if(strlen(dname) > ((sb->blksz) - (8*sizeof(uint64_t))))
	{
		errno = ENAMETOOLONG;
		return NULL;
	}

	// Procura o indice do inode de dname, e verifica se dname existe.
	uint64_t block = find_block(sb, dname, 0);
	if(block < 0)
	{
		errno = ENOENT;
		return NULL;
	}

	struct inode in;
	struct inode in_aux;
	struct nodeinfo ni;
	struct nodeinfo ni_aux;
	int aux, i;
	char* ret = (char*) malloc (500 * sizeof(char));
	char* tok;
	char name[50];

	// Posiciono e leio o inode de dname.
	lseek(sb->fd, block * sb->blksz, SEEK_SET);
	aux = read(sb->fd, &in, sb->blksz);

	// Verifica se o caminho dname aponta para um diretório.
	if(in.mode != IMDIR)
	{
		errno = ENOTDIR;
		return NULL;
	}

	// Posicionando e lendo o nodeinfo do diretório dname.
	lseek(sb->fd, in.meta * sb->blksz, SEEK_SET);
	aux = read(sb->fd, &ni, sb->blksz);

	// Percorrendo os links do diretório dname.
	for(i = 0; i < ni.size; i++)
	{
		// Leio o inode de cada arquivo/pasta dentro do diretorio dname.
		lseek(sb->fd, in.links[i], SEEK_SET);
		aux = read(sb->fd, &in_aux, sb->blksz);
		// Leio o nodeinfo desse inode.
		lseek(sb->fd, in_aux.meta, SEEK_SET);
		aux = read(sb->fd, &ni_aux, sb->blksz);
		// Pego o nome completo desse arquivo/pasta e divido ela em
		// substrings divididas pela /. Salvo a última parte.
		tok = strtok(ni_aux.name, "/");
		while(tok != NULL)
		{
			strcpy(name, tok);
			tok = strtok(NULL, "/");
		}
		if(in_aux.mode == IMDIR)
			strcat(name, "/");

		// Concateno o nome do arquivo/pasta com a string.
		strcat(ret, name);
		// Acrescento um espaço entre os arquivos/pastas.
		strcat(ret, " ");
	}
	return ret;
}
