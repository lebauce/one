/* -------------------------------------------------------------------------- */
/* Copyright 2002-2012, OpenNebula Project Leads (OpenNebula.org)             */
/*                                                                            */
/* Licensed under the Apache License, Version 2.0 (the "License"); you may    */
/* not use this file except in compliance with the License. You may obtain    */
/* a copy of the License at                                                   */
/*                                                                            */
/* http://www.apache.org/licenses/LICENSE-2.0                                 */
/*                                                                            */
/* Unless required by applicable law or agreed to in writing, software        */
/* distributed under the License is distributed on an "AS IS" BASIS,          */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   */
/* See the License for the specific language governing permissions and        */
/* limitations under the License.                                             */
/* -------------------------------------------------------------------------- */

#include <string>
#include <iostream>
#include <stdlib.h>

#include "VMTemplatePool.h"
#include "PoolTest.h"

using namespace std;

const int uids[] = {0,1,2};

const string names[] = {"Template one", "Second Template", "Third Template"};
const string mojo[] = {"1", "2", "3"};

const string templates[] =
{
    "NAME   = \"Template one\"\n"
    "MEMORY = 128\n"
    "CPU    = 1\n"
    "MOJO   = 1",

    "NAME   = \"Second Template\"\n"
    "MEMORY = 256\n"
    "CPU    = 2\n"
    "MOJO   = 2",

    "NAME   = \"Third Template\"\n"
    "MEMORY = 1024\n"
    "CPU    = 3\n"
    "MOJO   = 3",
};

class VMTemplatePoolFriend : public VMTemplatePool
{
public:
    VMTemplatePoolFriend(SqlDB * db) : VMTemplatePool(db){};

    int allocate(int    uid,
                 const  string& stemplate,
                 int *  oid)
     {
        VirtualMachineTemplate *    template_contents;
        char *                      error_msg = 0;
        int                         rc;
        string                      err;

        template_contents = new VirtualMachineTemplate();
        rc = template_contents->parse(stemplate,&error_msg);

        if( rc == 0 )
        {
            return VMTemplatePool::allocate(uid, 0,"the_user","oneadmin",template_contents, oid, err);
        }
        else
        {
            if (error_msg != 0 )
            {
                free(error_msg);
            }

            delete template_contents;
            return -2;
        }
    };
};

/* ************************************************************************* */
/* ************************************************************************* */

class VMTemplatePoolTest : public PoolTest
{
    CPPUNIT_TEST_SUITE (VMTemplatePoolTest);

    ALL_POOLTEST_CPPUNIT_TESTS();

    CPPUNIT_TEST ( names_initialization );
    CPPUNIT_TEST ( clone_template );
    CPPUNIT_TEST ( update );
    CPPUNIT_TEST ( get_using_name );
    CPPUNIT_TEST ( wrong_get_name );
    CPPUNIT_TEST ( duplicates );
    CPPUNIT_TEST ( name_index );
    CPPUNIT_TEST ( chown_name_index );

    CPPUNIT_TEST_SUITE_END ();

protected:

    void bootstrap(SqlDB* db)
    {
        VMTemplatePool::bootstrap(db);
    };

    PoolSQL* create_pool(SqlDB* db)
    {
        return new VMTemplatePoolFriend(db);
    };

    int allocate(int index)
    {
        int oid;
        return ((VMTemplatePoolFriend*)pool)->allocate(uids[index],
                                               templates[index],
                                               &oid);

    };

    void check(int index, PoolObjectSQL* obj)
    {
        string st;

        CPPUNIT_ASSERT( obj != 0 );

        VMTemplate* tmpl = static_cast<VMTemplate*>(obj);

        ObjectXML xml(tmpl->to_xml(st));

        CPPUNIT_ASSERT( tmpl->get_name() == names[index] );

        xml.xpath(st, "/VMTEMPLATE/TEMPLATE/MOJO", "-");
        CPPUNIT_ASSERT( st == mojo[index] );
    };

public:
    VMTemplatePoolTest(){xmlInitParser();};

    ~VMTemplatePoolTest(){xmlCleanupParser();};


    /* ********************************************************************* */

    void names_initialization()
    {
        VMTemplatePoolFriend *  tpool;
        VMTemplate *            temp;

        // Allocate 2 Templates, so they are written to the DB.
        allocate(0);
        allocate(2);

        // Create a new pool, using the same DB. This new pool should read the
        // allocated Templates.
        tpool = new VMTemplatePoolFriend(db);

        temp = tpool->get(names[0], uids[0], false);
        CPPUNIT_ASSERT( temp != 0 );

        temp = tpool->get(names[1], uids[1], false);
        CPPUNIT_ASSERT( temp == 0 );

        temp = tpool->get(names[2], uids[2], false);
        CPPUNIT_ASSERT( temp != 0 );


        delete tpool;
    }

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

    void clone_template()
    {
        string      attr_temp;
        string      attr_vmt;

        string      name1 = "NAME";
        string      name2 = "MEMORY";

        VMTemplatePoolFriend *   tpool;
        VMTemplate *             temp;
        VirtualMachineTemplate * vmt;

        int                     oid_1;

        tpool = static_cast<VMTemplatePoolFriend *>(pool);
        oid_1 = allocate(0);

        temp = tpool->get(oid_1, true);
        CPPUNIT_ASSERT( temp != 0 );

        vmt = temp->clone_template();

        vmt->get(name1,attr_vmt);
        CPPUNIT_ASSERT( attr_vmt == "Template one");

        temp->get_template_attribute(name1.c_str(), attr_temp);
        CPPUNIT_ASSERT( attr_temp == "Template one");

        temp->replace_template_attribute(name2.c_str(), "1024");

        vmt->get(name2,attr_vmt);
        CPPUNIT_ASSERT( attr_vmt == "128");

        temp->get_template_attribute(name2.c_str(), attr_temp);
        CPPUNIT_ASSERT( attr_temp == "1024");

        delete vmt;

        temp->get_template_attribute(name2.c_str(), attr_temp);
        CPPUNIT_ASSERT( attr_temp == "1024");

        tpool->update(temp);

        temp->unlock();
    };
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

    void update()
    {
        string      description_name    = "DESCRIPTION";
        string      description_val     = "";
        string      new_description     = "A new description";

        string      attr_name       = "NEW_ATTRIBUTE";
        string      attr_val        = "";
        string      new_attr_value  = "New value";

        string      no_value        = "Some random value";

        VMTemplatePoolFriend *  tpool;
        VMTemplate *            temp;
        int                     oid_1;

        tpool = static_cast<VMTemplatePoolFriend *>(pool);
        oid_1 = allocate(0);

        temp = tpool->get(oid_1, true);
        CPPUNIT_ASSERT( temp != 0 );

        // Object should be cached. Let's change some template attributes
        temp->replace_template_attribute(description_name, new_description);
        temp->replace_template_attribute(attr_name,        new_attr_value);
        temp->remove_template_attribute("ORIGINAL_PATH");

        tpool->update(temp);

        temp->unlock();

        temp = tpool->get(oid_1,false);
        CPPUNIT_ASSERT( temp != 0 );


        temp->get_template_attribute("DESCRIPTION",   description_val);
        temp->get_template_attribute("NEW_ATTRIBUTE", attr_val);
        temp->get_template_attribute("ORIGINAL_PATH", no_value);

        CPPUNIT_ASSERT( description_val == new_description );
        CPPUNIT_ASSERT( attr_val        == new_attr_value );
        CPPUNIT_ASSERT( no_value        == "" );

        //Now force access to DB

        pool->clean();
        temp = tpool->get(oid_1,false);

        CPPUNIT_ASSERT( temp != 0 );

        description_val = "";
        attr_val        = "";
        no_value        = "Random value";
        temp->get_template_attribute("DESCRIPTION",   description_val);
        temp->get_template_attribute("NEW_ATTRIBUTE", attr_val);
        temp->get_template_attribute("ORIGINAL_PATH", no_value);

        CPPUNIT_ASSERT( description_val == new_description );
        CPPUNIT_ASSERT( attr_val        == new_attr_value );
        CPPUNIT_ASSERT( no_value        == "" );
    };

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

    void get_using_name()
    {
        int oid_0, oid_1;

        VMTemplatePool * tpool = static_cast<VMTemplatePool*>(pool);

        // Allocate two objects
        oid_0 = allocate(0);
        oid_1 = allocate(1);

        // ---------------------------------
        // Get first object and check its integrity
        obj = tpool->get(oid_0, false);
        CPPUNIT_ASSERT( obj != 0 );
        check(0, obj);

        // Get using its name
        obj = tpool->get(names[1], uids[1], true);
        CPPUNIT_ASSERT( obj != 0 );
        obj->unlock();

        check(1, obj);


        // ---------------------------------
        // Clean the cache, forcing the pool to read the objects from the DB
        tpool->clean();

        // Get first object and check its integrity
        obj = tpool->get(names[0], uids[0], false);
        check(0, obj);

        // Get using its name
        obj = tpool->get(oid_1, false);
        check(1, obj);
    };

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

    void wrong_get_name()
    {
        VMTemplatePool * tpool = static_cast<VMTemplatePool*>(tpool);

        // The pool is empty
        // Non existing name
        obj = tpool->get("Wrong name", 0, true);
        CPPUNIT_ASSERT( obj == 0 );

        // Allocate an object
        allocate(0);

        // Ask again for a non-existing name
        obj = tpool->get("Non existing name",uids[0], true);
        CPPUNIT_ASSERT( obj == 0 );
    }

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

    void duplicates()
    {
        int rc, oid;
        VMTemplatePoolFriend * tpool = static_cast<VMTemplatePoolFriend*>(pool);

        // Allocate a template
        rc = tpool->allocate(uids[0], templates[0], &oid);
        CPPUNIT_ASSERT( oid == 0 );
        CPPUNIT_ASSERT( oid == rc );

        // Try to allocate twice the same template, should fail
        rc = tpool->allocate(uids[0], templates[0], &oid);
        CPPUNIT_ASSERT( rc  == -1 );
        CPPUNIT_ASSERT( oid == rc );

        // Try again, this time with different uid. Should be allowed
        rc = tpool->allocate(uids[1], templates[0], &oid);
        CPPUNIT_ASSERT( rc  >= 0 );
        CPPUNIT_ASSERT( oid == rc );
    }

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

    void name_index()
    {
        VMTemplatePool  *tpool = static_cast<VMTemplatePool*>(pool);
        VMTemplate      *vnet_oid, *vnet_name;
        int             oid_0;
        int             uid_0;
        string          name_0;

        oid_0 = allocate(0);

        CPPUNIT_ASSERT(oid_0 != -1);


        // ---------------------------------
        // Get by oid
        vnet_oid = tpool->get(oid_0, true);
        CPPUNIT_ASSERT(vnet_oid != 0);

        name_0 = vnet_oid->get_name();
        uid_0  = vnet_oid->get_uid();

        vnet_oid->unlock();

        // Get by name and check it is the same object
        vnet_name = tpool->get(name_0, uid_0, true);
        CPPUNIT_ASSERT(vnet_name != 0);
        vnet_name->unlock();

        CPPUNIT_ASSERT(vnet_oid == vnet_name);

        // ---------------------------------
        // Clean the cache, forcing the pool to read the objects from the DB
        tpool->clean();

        // Get by oid
        vnet_oid = tpool->get(oid_0, true);
        CPPUNIT_ASSERT(vnet_oid != 0);
        vnet_oid->unlock();

        // Get by name and check it is the same object
        vnet_name = tpool->get(name_0, uid_0, true);
        CPPUNIT_ASSERT(vnet_name != 0);
        vnet_name->unlock();

        CPPUNIT_ASSERT(vnet_oid == vnet_name);

        // ---------------------------------
        // Clean the cache, forcing the pool to read the objects from the DB
        tpool->clean();

        // Get by name
        vnet_name = tpool->get(name_0, uid_0, true);
        CPPUNIT_ASSERT(vnet_name != 0);
        vnet_name->unlock();

        // Get by oid and check it is the same object
        vnet_oid = tpool->get(oid_0, true);
        CPPUNIT_ASSERT(vnet_oid != 0);
        vnet_oid->unlock();

        CPPUNIT_ASSERT(vnet_oid == vnet_name);
    }

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

    void chown_name_index()
    {
        VMTemplatePool  *tpool = static_cast<VMTemplatePool*>(pool);
        VMTemplate      *obj_oid, *obj_name;
        int     oid;
        int     old_uid;
        int     new_uid = 3456;
        string  name;

        oid = allocate(0);

        CPPUNIT_ASSERT(oid != -1);


        // ---------------------------------
        // Get by oid
        obj_oid = tpool->get(oid, true);
        CPPUNIT_ASSERT(obj_oid != 0);

        name = obj_oid->get_name();
        old_uid  = obj_oid->get_uid();

        // Change owner and update cache index
        obj_oid->set_user(new_uid, "new_username");
        tpool->update(obj_oid);
        obj_oid->unlock();

        tpool->update_cache_index(name, old_uid, name, new_uid);

        // Get by name, new_uid and check it is the same object
        obj_name = tpool->get(name, new_uid, true);
        CPPUNIT_ASSERT(obj_name != 0);
        obj_name->unlock();

        CPPUNIT_ASSERT(obj_oid == obj_name);

        // Get by name, old_uid and check it does not exist
        obj_name = tpool->get(name, old_uid, true);
        CPPUNIT_ASSERT(obj_name == 0);

        // ---------------------------------
        // Clean the cache, forcing the pool to read the objects from the DB
        tpool->clean();


        // Get by name, old_uid and check it does not exist
        obj_name = tpool->get(name, old_uid, true);
        CPPUNIT_ASSERT(obj_name == 0);

        // Get by oid
        obj_oid = tpool->get(oid, true);
        CPPUNIT_ASSERT(obj_oid != 0);
        obj_oid->unlock();

        // Get by name, new_uid and check it is the same object
        obj_name = tpool->get(name, new_uid, true);
        CPPUNIT_ASSERT(obj_name != 0);
        obj_name->unlock();

        CPPUNIT_ASSERT(obj_oid == obj_name);
    }

    /* ********************************************************************* */

};

/* ************************************************************************* */
/* ************************************************************************* */

int main(int argc, char ** argv)
{
    return PoolTest::main(argc, argv, VMTemplatePoolTest::suite());
}
