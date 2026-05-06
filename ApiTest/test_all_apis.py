#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
TrainClient API Full Test Script
"""
import requests
import json
import time

BASE_URL = "http://127.0.0.1:8080"

def log_response(label, resp):
    print(f"Status: {resp.status_code}")
    try:
        print(f"Response: {resp.text[:500]}")
    except:
        print(f"Response: [binary or decode error]")

def test_login():
    print("\n" + "="*60)
    print("1. POST /api/auth/login")
    print("="*60)
    
    resp = requests.post(f"{BASE_URL}/api/auth/login", json={
        "username": "admin",
        "password": "admin"
    })
    log_response("login", resp)
    
    if resp.status_code == 200:
        data = resp.json()
        if data.get("success"):
            token = data.get("token")
            user = data.get("user", {})
            print(f"\n[OK] Login success")
            print(f"  Token: {token[:20]}...")
            print(f"  User: {user.get('username')}")
            return token
    print("\n[FAIL] Login failed")
    return None

def test_get_trains(token):
    print("\n" + "="*60)
    print("2. GET /api/trains")
    print("="*60)
    
    headers = {"Authorization": f"Bearer {token}"}
    resp = requests.get(f"{BASE_URL}/api/trains?date={time.strftime('%Y-%m-%d')}", headers=headers)
    log_response("get_trains", resp)
    
    if resp.status_code == 200:
        data = resp.json()
        if data.get("success"):
            trains = data.get("data", {}).get("trains", [])
            print(f"\n[OK] Success, {len(trains)} trains found")
            return True
    print("\n[FAIL]")
    return False

def test_get_trains_range(token):
    print("\n" + "="*60)
    print("3. GET /api/trains (range)")
    print("="*60)
    
    headers = {"Authorization": f"Bearer {token}"}
    now = int(time.time() * 1000)
    week_ago = now - 7 * 24 * 3600 * 1000
    
    resp = requests.get(f"{BASE_URL}/api/trains?startDate={week_ago}&endDate={now}", headers=headers)
    log_response("get_trains_range", resp)
    
    if resp.status_code == 200:
        data = resp.json()
        if data.get("success"):
            trains = data.get("data", {}).get("trains", [])
            print(f"\n[OK] Success, {len(trains)} trains found")
            return True
    print("\n[FAIL]")
    return False

def test_get_carriages(token):
    print("\n" + "="*60)
    print("4. GET /api/carriages")
    print("="*60)
    
    headers = {"Authorization": f"Bearer {token}"}
    resp = requests.get(f"{BASE_URL}/api/carriages?trainNumber=G4457&reachDatetime=1776905572", headers=headers)
    log_response("get_carriages", resp)
    
    if resp.status_code == 200:
        data = resp.json()
        if data.get("success"):
            carriages = data.get("data", {}).get("carriages", [])
            print(f"\n[OK] Success, {len(carriages)} carriages found")
            return True
    print("\n[FAIL]")
    return False

def test_submit_inspection(token):
    print("\n" + "="*60)
    print("5. PUT /api/carriages/1/inspection")
    print("="*60)
    
    headers = {"Authorization": f"Bearer {token}"}
    resp = requests.put(
        f"{BASE_URL}/api/carriages/1/inspection",
        headers=headers,
        json={"status": "examined", "remark": "test"}
    )
    log_response("submit_inspection", resp)
    
    if resp.status_code in [200, 201]:
        data = resp.json()
        if data.get("success"):
            print(f"\n[OK] Submit success")
            return True
    print("\n[FAIL]")
    return False

def test_get_users(token):
    print("\n" + "="*60)
    print("6. GET /api/users")
    print("="*60)
    
    headers = {"Authorization": f"Bearer {token}"}
    resp = requests.get(f"{BASE_URL}/api/users", headers=headers)
    log_response("get_users", resp)
    
    if resp.status_code == 200:
        data = resp.json()
        if data.get("success"):
            users = data.get("data", {}).get("users", [])
            print(f"\n[OK] Success, {len(users)} users found")
            return True
    print("\n[FAIL]")
    return False

def test_unauthorized():
    print("\n" + "="*60)
    print("7. GET /api/trains (no token)")
    print("="*60)
    
    resp = requests.get(f"{BASE_URL}/api/trains")
    log_response("unauthorized", resp)
    
    if resp.status_code == 401:
        print(f"\n[OK] Correctly rejected unauthorized")
        return True
    return False

def main():
    print("="*60)
    print("TrainClient API Full Test")
    print("="*60)
    print(f"Backend: {BASE_URL}")
    
    token = test_login()
    if not token:
        print("\n[FAIL] Login failed, stop testing")
        return
    
    test_get_trains(token)
    test_get_trains_range(token)
    test_get_carriages(token)
    test_submit_inspection(token)
    test_get_users(token)
    test_unauthorized()
    
    print("\n" + "="*60)
    print("Test Complete")
    print("="*60)

if __name__ == "__main__":
    main()
