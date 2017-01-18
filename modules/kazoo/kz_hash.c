#include "kz_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include "../../mem/shm_mem.h"
#include "../../hashes.h"
#include "../../dprint.h"
#include "../../str.h"

extern int dbk_command_table_size;

kz_amqp_cmd_table_ptr kz_cmd_htable = NULL;

int kz_hash_init()
{
	int i, j;

	if(kz_cmd_htable)
	{
		LM_ERR("already initialized");
		return 1;
	}
	
	i = 0;
	kz_cmd_htable = (kz_amqp_cmd_table_ptr)shm_malloc(dbk_command_table_size* sizeof(kz_amqp_cmd_table));
	if(kz_cmd_htable == NULL)
	{
		LM_ERR("memory error allocating command table");
		return 0;
	}
	memset(kz_cmd_htable, 0, dbk_command_table_size* sizeof(kz_amqp_cmd_table));

	for(i= 0; i< dbk_command_table_size; i++)
	{
		if(lock_init(&kz_cmd_htable[i].lock)== 0)
		{
			LM_ERR("initializing lock [%d]\n", i);
			goto error;
		}
		kz_cmd_htable[i].entries= (kz_amqp_cmd_entry_ptr)shm_malloc(sizeof(kz_amqp_cmd_entry));
		if(kz_cmd_htable[i].entries== NULL)
		{
			LM_ERR("memory error allocating command entry");
			return 0;
		}
		memset(kz_cmd_htable[i].entries, 0, sizeof(kz_amqp_cmd_entry));
		kz_cmd_htable[i].entries->next= NULL;
	}

	return 1;

error:
	if(kz_cmd_htable)
	{
		for(j=0; j< i; j++)
		{
			if(kz_cmd_htable[i].entries)
				shm_free(kz_cmd_htable[i].entries);
			else 
				break;
			lock_destroy(&kz_cmd_htable[i].lock);
		}
		shm_free(kz_cmd_htable);
	}
	return 0;

}

void kz_hash_destroy()
{
	int i;
	kz_amqp_cmd_entry_ptr p, prev_p;

	if(kz_cmd_htable== NULL)
		return;

	for(i= 0; i< dbk_command_table_size; i++)
	{
		lock_destroy(&kz_cmd_htable[i].lock);
		p= kz_cmd_htable[i].entries;
		while(p)
		{
			prev_p= p;
			p= p->next;
			kz_amqp_free_pipe_cmd(prev_p->cmd);
			shm_free(prev_p);
		}
	}
	shm_free(kz_cmd_htable);
}

kz_amqp_cmd_entry_ptr kz_search_cmd_table(str* message_id, unsigned int hash_code)
{
	kz_amqp_cmd_entry_ptr p;

	LM_DBG("searching %.*s\n", message_id->len,  message_id->s);
	p= kz_cmd_htable[hash_code].entries->next;
	while(p)
	{
		if(p->cmd->message_id->len== message_id->len &&
				strncmp(p->cmd->message_id->s, message_id->s, p->cmd->message_id->len) == 0 )
			return p;
		p= p->next;
	}
	return NULL;
}

int kz_cmd_store(kz_amqp_cmd_ptr cmd)
{
	unsigned int hash_code;
	kz_amqp_cmd_entry_ptr p= NULL;

	hash_code = core_hash(cmd->message_id, NULL, dbk_command_table_size);

	lock_get(&kz_cmd_htable[hash_code].lock);
	
	p= kz_search_cmd_table(cmd->message_id, hash_code);
	if(p)
	{
		LM_ERR("command already stored\n");
		lock_release(&kz_cmd_htable[hash_code].lock);
		return 0;
	}

	p = shm_malloc(sizeof(kz_amqp_cmd_entry));
	if(p== NULL)
	{
		lock_release(&kz_cmd_htable[hash_code].lock);
		LM_ERR("memory error allocation command pointer\n");
		return 0;
	}
	memset(p, 0, sizeof(kz_amqp_cmd_entry));

	p->cmd = cmd;
	p->next= kz_cmd_htable[hash_code].entries->next;
	kz_cmd_htable[hash_code].entries->next= p;

	lock_release(&kz_cmd_htable[hash_code].lock);
	
	return 1;
}

kz_amqp_cmd_ptr kz_cmd_retrieve(str* message_id)
{
	unsigned int hash_code;
	kz_amqp_cmd_entry_ptr p= NULL, prev_p= NULL;
	kz_amqp_cmd_ptr cmd = NULL;

	hash_code= core_hash(message_id, NULL, dbk_command_table_size);

	lock_get(&kz_cmd_htable[hash_code].lock);

	p = kz_search_cmd_table(message_id, hash_code);
	if(p== NULL)
	{
		LM_DBG("command pointer hash entry not found - %s\n", message_id->s);
		lock_release(&kz_cmd_htable[hash_code].lock);
		return NULL;
	}

	prev_p = kz_cmd_htable[hash_code].entries;
	while(prev_p->next)
	{
		if(prev_p->next== p)
			break;
		prev_p= prev_p->next;
	}
	if(prev_p->next== NULL)
	{
		LM_ERR("command pointer not found - %s\n", message_id->s);
		lock_release(&kz_cmd_htable[hash_code].lock);
		return NULL;
	}
	prev_p->next= p->next;
	cmd = p->cmd;
	shm_free(p);
	lock_release(&kz_cmd_htable[hash_code].lock);

	return cmd;
}


