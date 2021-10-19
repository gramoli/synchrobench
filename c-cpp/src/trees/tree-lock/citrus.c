#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "citrus.h" 
#include "urcu.h"

/**
 * Copyright 2014 Maya Arbel (mayaarl [at] cs [dot] technion [dot] ac [dot] il).
 * 
 * This file is part of Citrus. 
 * 
 * Citrus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * Author Maya Arbel
 */

/*struct node_t {
    int key;
    struct node_t* child[2];
	pthread_mutex_t lock;
	bool marked;
    int tag[2];
	int value; 
	};*/


node newNode(int key){
    node new = (node) malloc(sizeof(struct node_t));
	if( new==NULL){
		printf("out of memory\n");
		exit(1); 
	}    
	new->key=key;
    new->marked= false;
    new->child[0]=NULL;
    new->child[1]=NULL;
    new->tag[0]=0;
    new->tag[1]=0;
    if (pthread_mutex_init(&(new->lock), NULL) != 0){
        printf("\n mutex init failed\n");
    }
    return new;
}

node init(){
    node root = newNode(infinity);
	root->child[0]=newNode(infinity);
    return root;
}


int contains(node root, int key ){
	urcu_read_lock();
    node curr = root->child[0];
    int ckey = curr->key ;
    while (curr != NULL && ckey != key){
        if (ckey > key)
            curr = curr->child[0];
        if (ckey < key)
            curr = curr->child[1];
		if (curr!=NULL) 
                ckey = curr->key ;
    }
	urcu_read_unlock();
    if (curr == NULL) return -1;
    return 1;
}

bool validate(node prev,int tag ,node curr, int direction){
	bool result;     
	if (curr==NULL){
        result = (!(prev->marked) &&  (prev->child[direction]==curr) && (prev->tag[direction]==tag));
    }
	else {
		result = (!(prev->marked) && !(curr->marked) && prev->child[direction]==curr);
	}
	return result;
}

bool insert(node root, int key, int value){
    while(true){    
		urcu_read_lock();
        node prev = root;
        node curr = root->child[0];
        int direction = 0;
        int ckey = curr->key;
        int tag; 
        while (curr != NULL && ckey != key){
            prev = curr;
            if (ckey > key){
                curr = curr->child[0];
                direction = 0;
            }
            if (ckey < key){
                curr = curr->child[1];
                direction = 1;
            }
            if (curr!=NULL) 
                ckey = curr->key ;
        }
        tag = prev->tag[direction];
		urcu_read_unlock();
        if (curr!=NULL) return false;
        pthread_mutex_lock(&(prev->lock));
        if( validate(prev,tag,curr,direction) ){
            node new = newNode(key); 
			prev->child[direction]=new;

            pthread_mutex_unlock(&(prev->lock));
            return true;
        }
        pthread_mutex_unlock(&(prev->lock));
    }
}


bool delete(node root, int key){
    while(true){
		urcu_read_lock();    
        node prev = root;
        node curr = root->child[0];
        int direction = 0;
        int ckey = curr->key;
        while (curr != NULL && ckey != key){
            prev = curr;
            if (ckey > key){
                curr = curr->child[0];
                direction = 0;
            }
            if (ckey < key){
                curr = curr->child[1];
                direction = 1;
            }
            if (curr!=NULL) 
                ckey = curr->key ;
        }
        if (curr==NULL){
            urcu_read_unlock();
            return false;
        }         
		urcu_read_unlock();
        pthread_mutex_lock(&(prev->lock));
        pthread_mutex_lock(&(curr->lock));
        if( !validate(prev,0,curr,direction) ){
            pthread_mutex_unlock(&(prev->lock));
            pthread_mutex_unlock(&(curr->lock));
            continue;
        }
        if (curr->child[0] == NULL) {
            curr->marked=true;
            prev->child[direction]=curr->child[1];
            if(prev->child[direction] == NULL){
                prev->tag[direction]++;
            }
            pthread_mutex_unlock(&(prev->lock));
            pthread_mutex_unlock(&(curr->lock));
            return true;
        }
        if (curr->child[1] == NULL){
            curr->marked=true;
            prev->child[direction]=curr->child[0]; 
            if(prev->child[direction] == NULL){
                prev->tag[direction]++;
            }
            pthread_mutex_unlock(&(prev->lock));
            pthread_mutex_unlock(&(curr->lock));
            return true;
        }
		node prevSucc = curr;
        node succ = curr->child[1]; 
        
            node next = succ->child[0];
            while ( next!= NULL){
                prevSucc = succ;
                succ = next;
                next = next->child[0];
            }		
        int succDirection = 1; 
        if (prevSucc != curr){
            pthread_mutex_lock(&(prevSucc->lock));
            succDirection = 0;
        } 		
        pthread_mutex_lock(&(succ->lock));
        if (validate(prevSucc,0,succ, succDirection) && validate(succ,succ->tag[0],NULL, 0)){
            curr->marked=true;
            node new = newNode(succ->key);
            new->child[0]=curr->child[0];
            new->child[1]=curr->child[1];
            pthread_mutex_lock(&(new->lock)); 
            prev->child[direction]=new;  
            urcu_synchronize();
            if(prev->child[direction] == NULL){
                prev->tag[direction]++;
            }
            succ->marked=true;            
			if (prevSucc == curr){
                new->child[1]=succ->child[1];
                if(new->child[1] == NULL){
                    new->tag[1]++;
                }
            }
            else{
                prevSucc->child[0]=succ->child[1];
                if(prevSucc->child[1] == NULL){
                    prevSucc->tag[1]++;
                }
            }
			pthread_mutex_unlock(&(prev->lock));
            pthread_mutex_unlock(&(new->lock));            
			pthread_mutex_unlock(&(curr->lock));  	
            if (prevSucc != curr)
                pthread_mutex_unlock(&(prevSucc->lock));	
            pthread_mutex_unlock(&(succ->lock));
            return true; 
        }
        pthread_mutex_unlock(&(prev->lock));
        pthread_mutex_unlock(&(curr->lock));
        if (prevSucc != curr)
            pthread_mutex_unlock(&(prevSucc->lock));				
        pthread_mutex_unlock(&(succ->lock));
    }
}

