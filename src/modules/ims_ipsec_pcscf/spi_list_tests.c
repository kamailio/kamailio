/*
 * IMS IPSEC PCSCF module
 *
 * Copyright (C) 2018 Tsvetomir Dimitrov
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef _IPSEC_SPI_LIST_TEST

#include "spi_list.h"
#include <stdio.h>

void iterate(spi_list_t* list)
{
    spi_node_t* n = list->head;
    printf("HEAD: %d TAIL: %d; [", list->head->id, list->tail->id);

    while(n) {
        printf("%d ", n->id);
        n = n->next;
    }
    printf("]\n");
}

void check(spi_list_t* list, int* exp, int len, const char* func_name)
{
    //Special case for empty list
    if(len == 0) {
        if(list->head != NULL) {
            printf("%s: Empty list but head is not NULL.\n", func_name);
            return;
        }

        if(list->tail != NULL) {
            printf("%s: Empty list, but tail is not NULL\n", func_name);
            return;
        }

        goto success;
    }

    //Check head
    if(exp[0] != list->head->id) {
        printf("%s failed. Expected head: %d; Actual head: %d\n", func_name, exp[0], list->head->id);
        return;
    }

    //Check list contents
    spi_node_t* n = list->head;
    int i;
    for(i = 0; i < len; i++) {
        if(exp[i] != n->id) {
            printf("%s failed. list[%d] == %d; exp[%d] == %d\n", func_name, i, n->id, i, exp[i]);
            return;
        }
        n = n->next;
    }

    //Check tail
    if(exp[len-1] != list->tail->id) {
        printf("%s failed. Expected tail: %d; Actual tail: %d\n", func_name, exp[len-1], list->tail->id);
        return;
    }

success:
    printf("%s: OK\n", func_name);
}

void case1() // One element list
{
    spi_list_t list = create_list();

    int exp[] = {1};

    spi_add(&list, 1);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case2() // Two element list
{
    spi_list_t list = create_list();

    int exp[] = {1, 2};

    spi_add(&list, 1);
    spi_add(&list, 2);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case3() // Three element list
{
    spi_list_t list = create_list();

    int exp[] = {1, 2, 3};

    spi_add(&list, 1);
    spi_add(&list, 2);
    spi_add(&list, 3);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case4() // Delete head
{
    spi_list_t list = create_list();

    int exp[] = {2, 3};

    spi_add(&list, 1);
    spi_add(&list, 2);
    spi_add(&list, 3);

    spi_remove(&list, 1);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}


void case5() // Delete tail
{
    spi_list_t list = create_list();

    int exp[] = {1, 2 };

    spi_add(&list, 1);
    spi_add(&list, 2);
    spi_add(&list, 3);

    spi_remove(&list, 3);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case6() // Delete between
{
    spi_list_t list = create_list();

    int exp[] = {1, 3 };

    spi_add(&list, 1);
    spi_add(&list, 2);
    spi_add(&list, 3);

    spi_remove(&list, 2);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case7() // Out of order add
{
    spi_list_t list = create_list();

    int exp[] = {1, 2, 3};

    spi_add(&list, 2);
    spi_add(&list, 1);
    spi_add(&list, 3);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case8() //Random operations
{
    spi_list_t list = create_list();

    int exp[] = {1, 4, 6};

    spi_add(&list, 2);
    spi_add(&list, 1);
    spi_add(&list, 3);

    spi_remove(&list, 2);
    spi_add(&list, 4);
    spi_add(&list, 6);
    spi_remove(&list, 3);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case9() // Empty list
{
    spi_list_t list = create_list();

    int exp[] = {};

    spi_add(&list, 2);
    spi_add(&list, 1);
    spi_add(&list, 3);

    spi_remove(&list, 1);
    spi_remove(&list, 2);
    spi_remove(&list, 3);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}


void case10() //No duplicates
{
    spi_list_t list = create_list();

    int exp[] = {1,2,3};

    spi_add(&list, 1);
    spi_add(&list, 2);
    spi_add(&list, 2);
    spi_add(&list, 2);
    spi_add(&list, 3);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case11() //No duplicates
{
    spi_list_t list = create_list();

    int exp[] = {1,2,3};

    spi_add(&list, 1);
    spi_add(&list, 2);
    spi_add(&list, 3);
    spi_add(&list, 3);
    spi_add(&list, 3);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case12() //No duplicates
{
    spi_list_t list = create_list();

    int exp[] = {1,2,3};

    spi_add(&list, 1);
    spi_add(&list, 1);
    spi_add(&list, 2);
    spi_add(&list, 3);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case13() //No duplicates
{
    spi_list_t list = create_list();

    int exp[] = {1,2,3};

    spi_add(&list, 1);
    spi_add(&list, 2);
    spi_add(&list, 3);
    spi_add(&list, 1);

    check(&list, exp, sizeof(exp)/sizeof(int), __func__);

    destroy_list(list);
}

void case14()
{
    spi_list_t list = create_list();
    spi_add(&list, 2);
    spi_add(&list, 3);
    spi_add(&list, 5);
    spi_add(&list, 6);

    if(spi_in_list(&list, 1,1) != 0) {
        printf("%s: failed. 1 is not in list, but spi_in_list() returns true.\n", __func__);
        return;
    }

    if(spi_in_list(&list, 4,4) != 0) {
        printf("%s: failed. 4 is not in list, but spi_in_list() returns true.\n", __func__);
        return;
    }

    if(spi_in_list(&list, 7,7) != 0) {
        printf("%s: failed. 7 is not in list, but spi_in_list() returns true.\n", __func__);
        return;
    }

    if(spi_in_list(&list, 2,2) != 1) {
        printf("%s: failed. 2 is in list, but spi_in_list() returns false.\n", __func__);
        return;
    }

    if(spi_in_list(&list, 3,3) != 1) {
        printf("%s: failed. 3 is in list, but spi_in_list() returns false.\n", __func__);
        return;
    }

    if(spi_in_list(&list, 6,6) != 1) {
        printf("%s: failed. 6 is in list, but spi_in_list() returns false.\n", __func__);
        return;
    }

    printf("%s: OK\n", __func__);

    destroy_list(list);
}


int main()
{

    case1();
    case2();
    case3();
    case4();
    case5();
    case6();
    case7();
    case8();
    case9();
    case10();
    case11();
    case12();
    case13();
    case14();

    return 0;
}

#endif
